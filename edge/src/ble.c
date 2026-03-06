#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

#include "esp_log.h"
#include "esp_bt.h"
#include "nvs_flash.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "host/ble_hs.h"
#include "host/ble_gap.h"

#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "ble_gatt_client.h"
#include "ble.h"
#include "edge_config.h"
#include "auth.h"
// --------------------------------------------------------------------------
// >                   FwdDeclaration of functions                          <
// --------------------------------------------------------------------------

static void ble_tx_task(void *arg);
static void heartbeat_timer_cb(TimerHandle_t xTimer);

static void start_scan();
static void check_pairing_timeout();
static void pairing_timeout_cb(TimerHandle_t xTimer);

static int gap_event_connect(struct ble_gap_event *event);
static int gap_event_disconnect(struct ble_gap_event *event);
static int gap_event_disc(struct ble_gap_event *event);
static int gap_event_disc_complete(struct ble_gap_event *event);
static int gap_event_enc_change(struct ble_gap_event *event);
static int gap_event_notify(struct ble_gap_event *event);

// --------------------------------------------------------------------------
// >                           Configuration                                <
// --------------------------------------------------------------------------

#define MAX_NODES 10
#define ROAM_THRESHOLD 16
#define RSSI_SMOOTH_FACTOR 3
#define PAIRING_TIMEOUT 8000
#define ACK_TIMEOUT_MS 200

// --------------------------------------------------------------------------
// >                           Logging                                      <
// --------------------------------------------------------------------------

static const char *TAG = "BLE";

// --------------------------------------------------------------------------
// >                     BLE Node Tracking                                  <
// --------------------------------------------------------------------------
typedef struct {
    ble_addr_t addr;
    int8_t     rssi;
    bool       valid;
    uint32_t   last_seen;
    uint8_t    fail_count;
    uint32_t   blacklist_timer;
} node_info_t;

// --------------------------------------------------------------------------
// >                     BLE Scan Parameters                                <
// --------------------------------------------------------------------------

static struct ble_gap_disc_params scan_params = {
        .itvl = 0x40,
        .window = 0x20,
        .filter_policy = 0,
        .passive = 0,
        .limited = 0,
        .filter_duplicates = 1
};

// --------------------------------------------------------------------------
// >                     Global Runtime State                               <
// --------------------------------------------------------------------------
static node_info_t nodes[MAX_NODES];

static uint16_t current_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static ble_addr_t current_peer_addr;
static int current_conn_rssi = -127;
static int last_connect_index = -1;
static uint8_t own_addr_type;
static uint32_t last_roam_time = 0;



static TimerHandle_t pairing_timer;
static bool pairing_failed = false;
static uint32_t pairing_start_time = 0;
static uint8_t sequence_counter = 0;

ble_state_t ble_state = BLE_STATE_IDLE;

static volatile bool gatt_busy = false;
static edge_event_t tx_packet;
static bool tx_packet_pending = false;
static uint32_t tx_send_time = 0;
static uint32_t last_tx_time = 0;



// --------------------------------------------------------------------------
// >                     FreeRTOS Communication                             <
// --------------------------------------------------------------------------

static QueueHandle_t ble_tx_queue;
static TimerHandle_t heartbeat_timer;

static uint8_t get_node_id(const ble_addr_t *addr)
{
    return addr->val[5];
}

static int find_node_index(const ble_addr_t *addr)
{
    for (int i = 0; i < MAX_NODES; i++)
    {
        if (nodes[i].valid &&
            ble_addr_cmp(&nodes[i].addr, addr) == 0)
        {
            return i;
        }
    }
    return -1;
}

static void node_failure_tracker(int index)
{
    nodes[index].fail_count++;

    if(nodes[index].fail_count >= MAX_CONNECT_FAILS)
    {
        nodes[index].blacklist_timer =
            xTaskGetTickCount() + NODE_BLACKLIST_TIME;
        
        nodes[index].fail_count = 0;
        
        ESP_LOGW(TAG, "[NODE_%d|%02X] temporarily blacklisted",
                 index,
                 get_node_id(&nodes[index].addr));

        if (ble_gap_disc_active())
        {
            ble_gap_disc_cancel();
        }
        start_scan();
    }
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) 
    {
    
    case BLE_GAP_EVENT_CONNECT:
        return gap_event_connect(event);

    case BLE_GAP_EVENT_DISCONNECT:
        return gap_event_disconnect(event);

    case BLE_GAP_EVENT_DISC:
        return gap_event_disc(event);

    case BLE_GAP_EVENT_DISC_COMPLETE:
        return gap_event_disc_complete(event);
    
    case BLE_GAP_EVENT_ENC_CHANGE:
        return gap_event_enc_change(event);

    case BLE_GAP_EVENT_NOTIFY_RX:
        return gap_event_notify(event);
    
    default:
        return 0;
    }
}

static int gap_event_connect(struct ble_gap_event *event)
{
    struct ble_gap_conn_desc desc;
        
    if (event->connect.status == 0) 
    {
        ble_state = BLE_STATE_CONNECTED;
        ESP_LOGI(TAG, "Connected");
        pairing_failed = false;
            
        current_conn_handle = event->connect.conn_handle;
            
        ble_gap_conn_find(event->connect.conn_handle, &desc);
        ESP_LOGI(TAG, "Bonded: %d", desc.sec_state.bonded);
            
        

        if (!desc.sec_state.encrypted)
        {
            pairing_start_time = xTaskGetTickCount();
            xTimerStart(pairing_timer, 0);
            
            ble_gap_security_initiate(current_conn_handle);
        }    
        current_conn_rssi = -127;
            
        memcpy(&current_peer_addr,
               &desc.peer_ota_addr,
               sizeof(ble_addr_t));
        
        last_connect_index = -1;
    } 
    else
    {
        ESP_LOGE(TAG, "Connection failed");
            
        if (last_connect_index >= 0)
        {
            nodes[last_connect_index].fail_count++;
                
            if (nodes[last_connect_index].fail_count >= MAX_CONNECT_FAILS)
            {
                nodes[last_connect_index].blacklist_timer =
                    xTaskGetTickCount() + NODE_BLACKLIST_TIME;
                    
                nodes[last_connect_index].fail_count = 0;

                ESP_LOGW(TAG, "[NODE_%d|%02X] temp. blacklisted",
                         last_connect_index,
                         get_node_id(&nodes[last_connect_index].addr));
            }
            last_connect_index = -1;
            
        }
        current_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        ble_state = BLE_STATE_SCANNING;
        start_scan();
    }
    return 0;
}

static int gap_event_disconnect(struct ble_gap_event *event)
{
    xTimerStop(pairing_timer, 0);
    current_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    current_conn_rssi = -127;
        
    ESP_LOGI(TAG, "Disconnected");
        
    pairing_start_time = 0;
    
    ble_state = BLE_STATE_SCANNING;
    start_scan();
        
    return 0;
}

static int gap_event_disc(struct ble_gap_event *event)
{        
    struct ble_gap_disc_desc *desc = &event->disc;
    struct ble_hs_adv_fields fields;
    
    if (desc->rssi < -120 || desc->rssi > 20)
        return 0;
    
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
    
    int n_idx = find_node_index(&desc->addr);

    if (n_idx >= 0)
    {
        if (!nodes[n_idx].valid)
        {
            nodes[n_idx].rssi = desc->rssi;
        }
        else
        {
            nodes[n_idx].rssi = 
                (nodes[n_idx].rssi * (RSSI_SMOOTH_FACTOR -1) + desc->rssi) 
                / RSSI_SMOOTH_FACTOR;
        }
        nodes[n_idx].last_seen = xTaskGetTickCount();
                
        if (memcmp(&desc->addr, &current_peer_addr, sizeof(ble_addr_t)) == 0)
        {
            current_conn_rssi = nodes[n_idx].rssi;
        }
        uint32_t now = xTaskGetTickCount();

        bool same_node =
            ble_addr_cmp(&nodes[n_idx].addr, &current_peer_addr) == 0;
        
        bool strong_node =
            nodes[n_idx].rssi > current_conn_rssi + ROAM_THRESHOLD;

        bool current_node_isweak =
            current_conn_rssi < -75;
        
        bool roam_delay_ok =
            now - last_roam_time > pdMS_TO_TICKS(8000);

        if (ble_state == BLE_STATE_READY &&
            !gatt_busy &&
            current_conn_handle != BLE_HS_CONN_HANDLE_NONE &&
            !same_node &&
            strong_node &&
            current_node_isweak &&
            roam_delay_ok)
        {
            ESP_LOGI(TAG, "ROAMING -> [NODE_%d|%02X]: RSSI=%d > current %d", 
                     n_idx,
                     get_node_id(&nodes[n_idx].addr),
                     nodes[n_idx].rssi,
                     current_conn_rssi);
                
            last_roam_time = now;
            ble_gap_terminate(current_conn_handle,
                              BLE_ERR_REM_USER_CONN_TERM);
        }
        return 0;
    }

    int free_slot = -1;
    for (int i = 0; i < MAX_NODES; i++)
    {
        if (!nodes[i].valid)
        {
            free_slot = i;
            break;
        }
    }
    if (free_slot >= 0)
    {
        nodes[free_slot].addr = desc->addr;
        nodes[free_slot].rssi = desc->rssi;
        nodes[free_slot].valid = true;
        nodes[free_slot].last_seen = xTaskGetTickCount();
        nodes[free_slot].fail_count = 0;
        nodes[free_slot].blacklist_timer = 0;
    }
    else
    {
        int weakest = 0;
        for (int i = 1; i < MAX_NODES; i++)
        {
            if (nodes[i].rssi < nodes[weakest].rssi)
            {
                weakest = i;
            }
        }
        ESP_LOGD(TAG, "Replacing weakest node RSSI=%d", nodes[weakest].rssi);

        nodes[weakest].addr = desc->addr;
        nodes[weakest].rssi = desc->rssi;
        nodes[weakest].last_seen = xTaskGetTickCount();
        nodes[weakest].valid = true;
        nodes[weakest].fail_count = 0;
        nodes[weakest].blacklist_timer = 0;
    }
    return 0;
}

static int gap_event_disc_complete(struct ble_gap_event *event)
{
    int best_index = -1;
    int best_rssi = -127;
    uint32_t now = xTaskGetTickCount();

    for (int i = 0; i < MAX_NODES; i++)
    {   
        if (!nodes[i].valid)
            continue;

        if ((now - nodes[i].last_seen) > pdMS_TO_TICKS(6000))
        {
            nodes[i].valid = false;
            nodes[i].fail_count = 0;
            nodes[i].blacklist_timer = 0;
            continue;
        }
        if (nodes[i].blacklist_timer > now)
            continue;
            
        if (nodes[i].rssi > best_rssi)
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
            ESP_LOGI(TAG,
                     "Connecting -> [NODE_%d|%02X] RSSI=%d addr=%02X:%02X:%02X:%02X:%02X:%02X",
                     best_index,
                     get_node_id(&nodes[best_index].addr),
                     (int)best_rssi,
                     nodes[best_index].addr.val[5],
                     nodes[best_index].addr.val[4],
                     nodes[best_index].addr.val[3],
                     nodes[best_index].addr.val[2],
                     nodes[best_index].addr.val[1],
                     nodes[best_index].addr.val[0]);

            current_conn_rssi = nodes[best_index].rssi;
            
            last_connect_index = best_index;

            int rc = ble_gap_connect(own_addr_type,
                                     &nodes[best_index].addr,
                                     30000,
                                     NULL,
                                     gap_event,
                                     NULL);           
            if (rc == 0) 
            {
                ble_state = BLE_STATE_CONNECTING;
            }
        } 
    }
    if (ble_state != BLE_STATE_CONNECTING)
    {
        start_scan();
    }
    return 0;
}

static int gap_event_enc_change(struct ble_gap_event *event)
{
    ESP_LOGI(TAG, "enc status=%d", event->enc_change.status);
    gatt_client_reset();
        
    if (event->enc_change.status == 0)
    {
        xTimerStop(pairing_timer, 0);
        pairing_start_time = 0;
        
        int n_idx = find_node_index(&current_peer_addr);
        if (n_idx >= 0)
        {
            nodes[n_idx].fail_count = 0;
            nodes[n_idx].blacklist_timer = 0;    
        }
        
        ble_state = BLE_STATE_DISCOVERING;
        ESP_LOGI(TAG, "Encryption established");
        

        ble_gattc_disc_all_svcs(current_conn_handle,
                                gatt_svc_cb,
                                NULL);    
    }
    else
    {   
        xTimerStop(pairing_timer, 0);
        pairing_start_time = 0;

        int n_idx = find_node_index(&current_peer_addr);
        if (!pairing_failed && n_idx >= 0)
        {
            nodes[n_idx].fail_count++;

            if (nodes[n_idx].fail_count >= MAX_CONNECT_FAILS)
            {
                nodes[n_idx].blacklist_timer = 
                    xTaskGetTickCount() + NODE_BLACKLIST_TIME;
                
                nodes[n_idx].fail_count = 0;

                ESP_LOGW(TAG, "[Node_%d|%02X] Blacklisted: pairing failure", 
                         n_idx,
                         get_node_id(&nodes[n_idx].addr));
            }           
        }
        pairing_failed = false;

        if (current_conn_handle != BLE_HS_CONN_HANDLE_NONE)
        {
            ble_gap_terminate(current_conn_handle,
                              BLE_ERR_REM_USER_CONN_TERM);
        }  
        current_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        ble_state = BLE_STATE_SCANNING;
    }
    return 0;
} 

static int gap_event_notify(struct ble_gap_event *event)
{
    struct os_mbuf *om = event->notify_rx.om;

    int len = OS_MBUF_PKTLEN(om);
    uint8_t data[32];

    if (len > sizeof(data))
    {
        ESP_LOGW(TAG, "Notification too big: %d", len);
        len = sizeof(data);
    }
    os_mbuf_copydata(om, 0, len, data);

    ESP_LOGI(TAG, "ACK seq=%d status =%d", data[0], data[1]);
    
    uint8_t status = data[1];

    if (status == 1)
    {
        tx_packet_pending = false;
    }
    gatt_busy = false;

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
    ble_state = BLE_STATE_SCANNING;
    start_scan(); 
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
    
    ESP_LOGI(TAG, "Initializing NimBLE...");
    nimble_port_init();
    
    ble_svc_gap_init();
    ble_svc_gatt_init();
    gatt_client_init();

    ble_svc_gap_device_name_set("H2_EDGE");

    ble_hs_cfg.sync_cb = ble_app_on_sync;
    // Enable Secure connection (BLE)
    ble_hs_cfg.sm_sc = 1;

// Enable bonding (store keys in NVS)
    ble_hs_cfg.sm_bonding = 1;

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
    
    ble_tx_queue = xQueueCreate(20, sizeof(ble_tx_msg_t));
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
    
    pairing_timer = xTimerCreate("pair_timeout",
                                 pdMS_TO_TICKS(PAIRING_TIMEOUT),
                                 pdFALSE,
                                 NULL,
                                 pairing_timeout_cb);
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

    if (xQueueSend(ble_tx_queue, &msg, 0) != pdTRUE)
    {
        ESP_LOGW(TAG, "TX queue full");
    }
}

static void ble_tx_task(void *arg)
{
    ble_tx_msg_t msg;

    while(1) {
        if (!tx_packet_pending)
        {
            if (xQueueReceive(ble_tx_queue, &msg, portMAX_DELAY))
            {
                tx_packet = msg.event;
                tx_packet_pending = true;

                ESP_LOGI(TAG, "TX queued seq=%d", tx_packet.seq);
            }
        }
        uint32_t now = xTaskGetTickCount();

        bool ack_timeout = 
            gatt_busy &&
            (now - last_tx_time) > pdMS_TO_TICKS(ACK_TIMEOUT_MS);

        if (tx_packet_pending &&
            ble_state == BLE_STATE_READY &&
            current_conn_handle != BLE_HS_CONN_HANDLE_NONE &&
            (!gatt_busy || ack_timeout))
        {
            ESP_LOGI(TAG, "TX sending seq=%d", tx_packet.seq);

            gatt_busy = true;
            last_tx_time = xTaskGetTickCount();
            int rc = gatt_send_event(current_conn_handle, &tx_packet);

            if (rc != 0)
            {
                gatt_busy = false;
                ESP_LOGW(TAG, "TX failed, will retry");
            }
        }
        if (ack_timeout)
        {
            if((xTaskGetTickCount() - tx_send_time) > pdMS_TO_TICKS(4000))
            {
                ESP_LOGW(TAG, "ACK timeout, retrying seq=%d", tx_packet.seq);
                gatt_busy = false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
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
    start_scan();
}

static void start_scan()
{
    if(ble_state == BLE_STATE_CONNECTING)
    {
        return;
    }
    int rc = 0;

    if (ble_gap_disc_active())
        return;

    rc = ble_gap_disc(own_addr_type,
                       SCAN_LENGTH,
                       &scan_params,
                       gap_event,
                       NULL);
    if (rc != 0 && rc != BLE_HS_EBUSY)
    {
        ESP_LOGW(TAG, "Scan start failed: %d", rc);
    }
}

static void check_pairing_timeout()
{
    if (ble_state == BLE_STATE_CONNECTED ||
        ble_state == BLE_STATE_DISCOVERING)
    {
        uint32_t now = xTaskGetTickCount();

        if (pairing_start_time &&
            (now - pairing_start_time) > pdMS_TO_TICKS(15000))
        {
            ESP_LOGW(TAG, "Pairing timeout, restarting connection");

            if (current_conn_handle != BLE_HS_CONN_HANDLE_NONE)
            {
                ble_gap_terminate(current_conn_handle,
                                  BLE_ERR_REM_USER_CONN_TERM);
            }
            pairing_start_time = 0;
        }
    }
}

static void pairing_timeout_cb(TimerHandle_t xTimer)
{
    ESP_LOGW(TAG, "Pairing timeout");

    int n_idx = find_node_index(&current_peer_addr);
    if (n_idx >= 0)
    {   
        pairing_failed = true;
        node_failure_tracker(n_idx);
    }

    if (current_conn_handle != BLE_HS_CONN_HANDLE_NONE)
    {
        ble_gap_terminate(current_conn_handle,
                          BLE_ERR_REM_USER_CONN_TERM);
    }
    pairing_start_time = 0;
}