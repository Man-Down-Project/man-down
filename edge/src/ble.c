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

#include "ble_gatt_client.h"
#include "ble.h"
#include "edge_protocol.h"

static ble_state_t ble_state = BLE_STATE_IDLE;
// struct ble_gap_adv_params adv_params = {
//         .conn_mode = BLE_GAP_CONN_MODE_UND,
//         .disc_mode = BLE_GAP_DISC_MODE_GEN,
// };
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

#define RECONNECT_DELAY_MIN 2000
#define RECONNECT_DELAY_MAX 30000

// varibles used for best RSSI pick
static int best_rssi = -127;
static ble_addr_t best_addr;
static int best_found = 0;
static uint16_t current_conn_handle;



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
            
        } else {
            ESP_LOGE(TAG, "Connection failed");
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ble_state = BLE_STATE_SCANNING;
        ESP_LOGI(TAG, "Disconnected");

        vTaskDelay(pdMS_TO_TICKS(reconnect_delay_ms));
        
        ble_gap_disc(own_addr_type,
                     300, 
                     &scan_params,
                     gap_event,
                     NULL);
        break;

    case BLE_GAP_EVENT_DISC:
        
        struct ble_hs_adv_fields fields;
        
        
        struct ble_gap_disc_desc *desc = &event->disc;
        int rc = ble_hs_adv_parse_fields(&fields,
                                         desc->data,
                                         desc->length_data);
        char uuid_str[BLE_UUID_STR_LEN];
        // char target_str[BLE_UUID_STR_LEN];

        // ble_uuid_to_str(gatt_get_service_uuid(), target_str);
        // ESP_LOGI(TAG, "Target UUID: %s", target_str);

        // if (rc == 0 && fields.uuids128 != NULL) {
        //     for (int i = 0; i < fields.num_uuids128; i++) {
        //         ble_uuid_to_str(&fields.uuids128[i].u, uuid_str);
        //         ESP_LOGI(TAG, "Advertised UUID: %s", uuid_str);
        //     }
        // }
        if (rc == 0 && fields.uuids128 != NULL) {
            for (int i = 0; i < fields.num_uuids128; i++) {
                if (ble_uuid_cmp(&fields.uuids128[i].u, gatt_get_service_uuid()) == 0) {
                    ESP_LOGI(TAG, "Found device RSSI: %d", desc->rssi);

                    if (desc->rssi > best_rssi) {
                    best_rssi = desc->rssi;
                    best_addr = desc->addr;
                    best_found = 1;

                    }
                }
            }

        
        }
        break;
    
    case BLE_GAP_EVENT_DISC_COMPLETE:
    {

        if (best_found) {
            ESP_LOGI(TAG, "Best RSSI: %d ,connecting...",best_rssi);

            ble_gap_connect(own_addr_type,
                            &best_addr,
                            30000,
                            NULL,
                            gap_event,
                            NULL);
            best_rssi = -127;
            best_found = 0;
        } else {
            ESP_LOGI(TAG, "No device found, rescanning");

            ble_state = BLE_STATE_SCANNING;

            ble_gap_disc(own_addr_type,
                         300,
                         &scan_params,
                         gap_event,
                         NULL);
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
    ble_state = BLE_STATE_SCANNING;
    int rc;

    rc = ble_hs_id_infer_auto(0, &own_addr_type);

    if (rc != 0) {
        ESP_LOGE(TAG, "Address infer failed");
        return;
    }

    rc = ble_gap_disc(own_addr_type,
                      300,
                      &scan_params,
                      gap_event,
                      NULL);
    
    
    if (rc != 0) {
        ESP_LOGE(TAG, "Scan start failed:: %d", rc);
        return;
    } else {
        ESP_LOGI(TAG, "Scanning started");
    }
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


    ESP_LOGI(TAG, "Initializing Bluetooth Controller");

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    ESP_ERROR_CHECK(esp_nimble_init());
    ESP_LOGI(TAG, "Initializing NimBLE...");

    gatt_client_init();
    nimble_port_init();

    ble_svc_gap_init();
    ble_svc_gatt_init();

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

    nimble_port_freertos_init(ble_host_task);
}

