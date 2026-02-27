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


#include "ble.h"

struct ble_gap_adv_params adv_params = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
};

static const char *TAG = "BLE";
static uint8_t own_addr_type;

// GAP event handler
static int gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) 
    {

    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            ESP_LOGI(TAG, "Connected");

            //Start secuirity (this triggers pairing if needed)
            ble_gap_security_initiate(event->connect.conn_handle);
        } else {
            ESP_LOGE(TAG, "Connection failed");
        }       
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Disconnected");

        ble_gap_adv_start(own_addr_type,
                          NULL,
                          BLE_HS_FOREVER,
                          &adv_params,
                          gap_event,
                          NULL);
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "Advertising complete");
        break;
    
    case BLE_GAP_EVENT_ENC_CHANGE:
        if (event->enc_change.status == 0) {
            ESP_LOGI(TAG, "Encryption established");
        } else {
            ESP_LOGE(TAG, "Encryption failed");
        }
        break;

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

    
    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));

    fields.name = (uint8_t *)"H2_EDGE";
    fields.name_len = strlen("H2_EDGE");
    fields.name_is_complete = 1;

    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                   BLE_HS_ADV_F_BREDR_UNSUP;
    
    ble_gap_adv_set_fields(&fields);
    
    rc = ble_gap_adv_start(own_addr_type,
                           NULL,
                           BLE_HS_FOREVER,
                           &adv_params,
                           gap_event,
                           NULL);
    
    if (rc != 0) {
        ESP_LOGE(TAG, "Advertising start failed: %d", rc);
        return;
    }
    
    ESP_LOGI(TAG, "Advertising started");
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

    nimble_port_init();

    ble_svc_gap_init();
    ble_svc_gatt_init();

    ble_svc_gap_device_name_set("H2_EDGE");

    ble_hs_cfg.sync_cb = ble_app_on_sync;
    // Enable Secure connection (BLE)
    ble_hs_cfg.sm_sc = 1;

// Enable bonding (store keys in NVS)
    ble_hs_cfg.sm_bonding = 1;

// No MITM (not sure what this does yet.)
    ble_hs_cfg.sm_mitm = 0;

// No input/output capability
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;

// Key distribution
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;

    nimble_port_freertos_init(ble_host_task);
}
