#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include <string.h>

#include "ble_gatt_client.h"
#include "ble.h"
#include "edge_config.h"
#include "auth.h"

// func declaration
static void ble_tx_task(void *arg);
static void heartbeat_timer_cb(TimerHandle_t xTimer);
//void ble_send_event(const edge_event_t *event);


// queue variables
static edge_event_t pending_event;
static bool event_pending = false;
// RTOS task queue variables
static QueueHandle_t ble_tx_queue;
static TimerHandle_t heartbeat_timer;

ble_state_t ble_state = BLE_STATE_IDLE;
struct ble_gap_disc_params scan_params = {
        .itvl = 0x50,
        .window = 0x30,
        .filter_policy = 0,
        .passive = 0,
        .limited = 0,
        .filter_duplicates = 1
};
static const char *TAG = "BLE";
static uint8_t own_addr_type;

// variables to handle retry if encryption failed
static uint8_t enc_fail_count = 0;
static uint32_t reconnect_delay_ms = 2000;
static uint8_t sequence_counter = 0;

#define RECONNECT_DELAY_MIN 2000
#define RECONNECT_DELAY_MAX 30000

// varibles used for best RSSI pick (edge -> node)
#define MAX_NODES 10
#define ROAM_THRESHOLD 12 // Used in the descision to swap node or not (hysteresis)

typedef struct {
    ble_addr_t addr;
    int rssi;
    bool valid;
} node_info_t;

static node_info_t nodes[MAX_NODES];
static int current_conn_rssi = -127;
static uint16_t current_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static ble_addr_t current_peer_addr;





// GAP event handler
static int gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) 
    {
    
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            
            ble_state = BLE_STATE_CONNECTED;
            ESP_LOGI(TAG, "Connected");
            
            struct ble_gap_conn_desc desc;
            current_conn_handle = event->connect.conn_handle;
            
            ble_gap_conn_find(event->connect.conn_handle, &desc);
            ESP_LOGI(TAG, "Bonded: %d", desc.sec_state.bonded);

            ble_gap_security_initiate(current_conn_handle);
            
            current_conn_rssi = -127;
            
            memcpy(&current_peer_addr,
                   &desc.peer_ota_addr,
                    sizeof(ble_addr_t));
            
        } else {
            ESP_LOGE(TAG, "Connection failed");
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        
        current_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        
        current_conn_rssi = -127;
        
        ESP_LOGI(TAG, "Disconnected");
        ESP_LOGI(TAG, "Scanning...");
        for (int i = 0; i < MAX_NODES; i++)
        {
            nodes[i].valid = false;
        }

        ble_state = BLE_STATE_SCANNING;
        
        int rc = ble_gap_disc(own_addr_type,
                          1000,
                          &scan_params,
                          gap_event,
                          NULL);
        if (rc != 0)
        {
            ESP_LOGW(TAG, "Scan start failed: %d", rc);
        }
        break;

    case BLE_GAP_EVENT_DISC:
    {    
        struct ble_gap_disc_desc *desc = &event->disc;
        struct ble_hs_adv_fields fields;
        int rc = ble_hs_adv_parse_fields(&fields, desc->data, desc->length_data);

        if (rc != 0)
        {
            return 0;
        }

        bool match = false;

        for (int i = 0; i < fields.num_uuids128; i++)
        {
            if (ble_uuid_cmp(&fields.uuids128[i].u,
                             gatt_get_service_uuid()) == 0)
            {
                match = true;
                break;
            }
        }
        if (!match)
        {
            return 0;
        }
        
        for (int i = 0; i < MAX_NODES; i++)
        {
            if (nodes[i].valid &&
                memcmp(&nodes[i].addr, &desc->addr, sizeof(ble_addr_t)) == 0)
            {
                nodes[i].rssi = (nodes[i].rssi * 3 + desc->rssi) / 4;
                
                if (memcmp(&desc->addr, &current_peer_addr, sizeof(ble_addr_t)) == 0)
                {
                    current_conn_rssi = nodes[i].rssi;
                }
                goto done;
            }
        }

        for (int i = 0; i < MAX_NODES; i++)
        {
            if (!nodes[i].valid)
            {
                nodes[i].addr = desc->addr;
                nodes[i].rssi = desc->rssi;
                nodes[i].valid = true;

                if (memcmp(&desc->addr, &current_peer_addr, sizeof(ble_addr_t)) == 0)
                {
                    current_conn_rssi = desc->rssi;
                }
                break;
            }
        }
            
    done:
        break;
    }
    break;

    case BLE_GAP_EVENT_DISC_COMPLETE:
    {
        
        int best_index = -1;
        int best_rssi = -127;

        for (int i = 0; i < MAX_NODES; i++)
        {
            if (nodes[i].valid && nodes[i].rssi > best_rssi)
            {
                best_rssi = nodes[i].rssi;
                best_index = i;         
            }
        }
        // READING RSSI FROM CURRENT CONNECTION
        if (current_conn_handle != BLE_HS_CONN_HANDLE_NONE)
        {
            int8_t rssi;

            int rc = ble_gap_conn_rssi(current_conn_handle, &rssi);
            if (rc == 0)
            {
                current_conn_rssi = rssi;
            }
        }

        ESP_LOGI(TAG, "Scan finished. Best RSSI=%d current RSSI=%d",
                 best_rssi, current_conn_rssi);
        
        if (best_index >= 0)
        {
            if (current_conn_handle == BLE_HS_CONN_HANDLE_NONE)
            {
                ESP_LOGI(TAG, "Connecting to node RSSI=%d", best_rssi);

                current_conn_rssi = nodes[best_index].rssi;

                int rc = ble_gap_connect(own_addr_type,
                                         &nodes[best_index].addr,
                                         30000,
                                         NULL,
                                         gap_event,
                                         NULL);
                
                if (rc != 0)
                {
                    ESP_LOGW(TAG, "Connect start failed: %d", rc);
                }
                ble_state = BLE_STATE_CONNECTING;
            } else {
                
                if (current_conn_rssi != -127 &&
                    best_rssi > current_conn_rssi + ROAM_THRESHOLD)
                {
                    ESP_LOGI(TAG, "Roaming to stronger node");

                    ble_gap_terminate(current_conn_handle,
                                      BLE_ERR_REM_USER_CONN_TERM);
                } 
            }
        }
        
        
        int rc = ble_gap_disc(own_addr_type,
                              1000,
                              &scan_params,
                              gap_event,
                              NULL);
        if (rc != 0)
        {
            ESP_LOGW(TAG, "Scan restart failed: %d", rc);
        }
        
        break;
    }
    
    case BLE_GAP_EVENT_ENC_CHANGE:
        ESP_LOGI(TAG, "enc status=%d", event->enc_change.status);
        gatt_client_reset();
        if (event->enc_change.status == 0) {
            
            enc_fail_count = 0;
            reconnect_delay_ms = RECONNECT_DELAY_MIN;

            ble_state = BLE_STATE_DISCOVERING;
            ESP_LOGI(TAG, "Encryption established");

            ble_gattc_disc_all_svcs(current_conn_handle,
                                    gatt_svc_cb,
                                    NULL);
            
        } else {
            ESP_LOGE(TAG, "Encryption failed");
            enc_fail_count++;

            if (enc_fail_count > 5) 
            {
                ESP_LOGE(TAG, "Too many encryption failures, giving up...");
                ble_state = BLE_STATE_IDLE;
                break;
            }

            reconnect_delay_ms = RECONNECT_DELAY_MIN << enc_fail_count;

            if (reconnect_delay_ms > RECONNECT_DELAY_MAX) {
                reconnect_delay_ms = RECONNECT_DELAY_MAX;
            }

            ESP_LOGW(TAG, "Retry in %lu ms (fail count=%d)",
                     reconnect_delay_ms, enc_fail_count);
            
            ble_gap_terminate(current_conn_handle,
                              BLE_ERR_REM_USER_CONN_TERM);
        }
        break;

    case BLE_GAP_EVENT_NOTIFY_RX:
    {
        struct os_mbuf *om = event->notify_rx.om;

        int len = OS_MBUF_PKTLEN(om);

        ESP_LOGI(TAG, "Notification received, len=%d", len);

        uint8_t data[len];
        os_mbuf_copydata(om, 0, len, data);

        ESP_LOGI(TAG, "ACK seq=%d status =%d", data[0], data[1]);

        break;
    }

    default:
        break;
    }
    return 0;
}

static void ble_app_on_sync(void)
{
    
    int rc;

    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Address infer failed");
        return;
    }

    ESP_LOGI(TAG, "BLE stack synchronized");

     rc = ble_gap_disc(own_addr_type,
                       1000,
                       &scan_params,
                       gap_event,
                       NULL);
    if (rc != 0)
    {
        ESP_LOGW(TAG, "Scan start failed: %d", rc);
    }
    
    ble_state = BLE_STATE_SCANNING;
}

static void ble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void ble_init()
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing NVS");
    ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    // ESP_ERROR_CHECK(
    //     esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT)
    // );

    ESP_LOGI(TAG, "Initializing NimBLE...");
    //ESP_ERROR_CHECK(esp_nimble_init());
    // esp_err_t err = esp_nimble_init();
    // ESP_LOGI(TAG, "esp_nimble_init returned %d", err);
    nimble_port_init();
    
    
    
    ble_svc_gap_init();
    ble_svc_gatt_init();
    gatt_client_init();

    ble_svc_gap_device_name_set("H2_EDGE");

    ble_hs_cfg.sync_cb = ble_app_on_sync;
    // Enable Secure connection (BLE)
    ble_hs_cfg.sm_sc = 1;

// Enable bonding (store keys in NVS)
    ble_hs_cfg.sm_bonding = 0;

// No MITM (not sure what this does yet. guessing on = 1 off = 0)
// defaults back to legacy if not supported by the device
    ble_hs_cfg.sm_mitm = 0;

// No input/output capability
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;

// Key distribution
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;
    // =======================================
    //             START QUEUE + TASK
    // =======================================
    
    ble_tx_queue = xQueueCreate(5, sizeof(ble_tx_msg_t));
    if (ble_tx_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create TX queue");
    }
    xTaskCreate(ble_tx_task,
                "ble_tx",
                4096,
                NULL,
                5,
                NULL);
    
    heartbeat_timer = xTimerCreate("hb",
                                   pdMS_TO_TICKS(HEART_TIMER),
                                   pdTRUE,
                                   NULL,
                                   heartbeat_timer_cb);
    xTimerStart(heartbeat_timer, 0);
    

    // ========================================
    // Start NimBLE host task (need to be last)
    // ========================================
    nimble_port_freertos_init(ble_host_task);
}

uint16_t ble_get_conn_handle()
{
    return current_conn_handle;
}

void ble_send_event(const edge_event_t *event)
{
    ble_tx_msg_t msg;
    msg.event = *event;

    xQueueSend(ble_tx_queue, &msg, 0);
}

static void ble_tx_task(void *arg)
{
    ble_tx_msg_t msg;

    while(1) {
        if (xQueueReceive(ble_tx_queue, &msg, portMAX_DELAY)) 
        {
            ESP_LOGI(TAG, "TX requested");
            
            while (!ble_hs_is_enabled())
            {
                vTaskDelay(pdMS_TO_TICKS(100));
            }

            while (ble_state != BLE_STATE_READY)
            {
                vTaskDelay(pdMS_TO_TICKS(50));
            }
            gatt_send_event(current_conn_handle, &msg.event);
        }
    }
}

static void heartbeat_timer_cb(TimerHandle_t xTimer)
{
    edge_event_t event;
    
    event.device_id = DEVICE_ID;
    event.event_type = EVENT_HEARTBEAT;
    event.event_location = 0;
    event.battery_status = BATTERY_STATUS; // Should make a func to gather battery info (don't think we have adapter for battery power)
    event.seq = sequence_counter++;
    memset(event.auth_tag,0,AUTH_TAG_LEN);

    generate_auth_tag((uint8_t*)&event,
                      sizeof(edge_event_t) - AUTH_TAG_LEN,
                      event.auth_tag
    );
    ble_send_event(&event);
    
    // ESP_LOGI(TAG,"Struct size: %d\n", sizeof(edge_event_t));
    // ESP_LOGI(TAG,"Sending size: %d\n", sizeof(event));
    // ESP_LOGI(TAG,"Packet size: %d\n", sizeof(edge_event_t));

   
    
}

void ble_on_ready(uint16_t conn_handle)
{
    ble_state = BLE_STATE_READY;

    if(event_pending)
    {
        gatt_send_event(conn_handle, &pending_event);
        event_pending = false;
    }

    int rc = ble_gap_disc(own_addr_type,
                          1000,
                          &scan_params,
                          gap_event,
                          NULL);
    if (rc != 0)
    {
        ESP_LOGW(TAG, "Background scan failed: %d", rc);
    }
}