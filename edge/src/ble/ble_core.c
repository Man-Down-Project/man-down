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


#include "config/edge_config.h"
#include "ble/ble.h"
#include "ble/ble_internal.h"
#include "ble/ble_gap.h"
#include "ble/ble_tx.h"
#include "ble/ble_gatt_client.h"

// --------------------------------------------------------------------------
// >                           Logging                                      <
// --------------------------------------------------------------------------

static const char *TAG = "BLE";

// --------------------------------------------------------------------------
// >                     Global Runtime State                               <
// --------------------------------------------------------------------------

uint16_t current_conn_handle = BLE_HS_CONN_HANDLE_NONE;
ble_addr_t current_peer_addr;
int last_connect_index = -1;
int current_conn_rssi = -127;
uint8_t own_addr_type;
ble_state_t ble_state = BLE_STATE_IDLE;




void ble_on_ready(uint16_t conn_handle)
{
    ble_state = BLE_STATE_READY;
    start_scan();
}
uint16_t ble_get_conn_handle()
{
    return current_conn_handle;
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


