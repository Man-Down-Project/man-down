#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"

static const char *TAG = "GATT";
// GATT variables
static uint16_t target_char_handle = 0;

// Func declarations
static int gatt_write_cb(uint16_t conn_handle,
                         const struct ble_gatt_error *error,
                         struct ble_gatt_attr *attr,
                         void *arg);
static int gatt_chr_cb(uint16_t conn_handle,
                       const struct ble_gatt_error *error,
                       const struct ble_gatt_chr *chr,
                       void *arg);




int gatt_svc_cb(uint16_t conn_handle,
                     const struct ble_gatt_error *error,
                     const struct ble_gatt_svc *service,
                     void *arg)
{
    if (error->status == 0) {

        ESP_LOGI(TAG, "Service found: start=%d end=%d",
                 service->start_handle,
                 service->end_handle);
        

        // Startar characteristic discovery inom denna service ("filen i mappen")
        ble_gattc_disc_all_chrs(conn_handle,
                                service->start_handle,
                                service->end_handle,
                                gatt_chr_cb,
                                NULL);
    }
    else if (error->status == BLE_HS_EDONE) {
        ESP_LOGI(TAG, "Service discovery complete");
    } else {
        ESP_LOGE(TAG, "Service discovery error: %d", error->status);
    }
    return 0;
}

static int gatt_chr_cb(uint16_t conn_handle,
                       const struct ble_gatt_error *error,
                       const struct ble_gatt_chr *chr,
                       void *arg)
{
    if (error->status == 0) {

        ESP_LOGI(TAG, "Characteristic found: def_handle=%d val_handle=%d",
                 chr->def_handle,
                 chr->val_handle);

        if (ble_uuid_cmp(&chr->uuid.u, BLE_UUID16_DECLARE(0xA103)) == 0) {
            target_char_handle = chr->val_handle;
            ESP_LOGI(TAG, "Target characteristic found!");
        }
    }
    else if (error->status == BLE_HS_EDONE) {
        
        ESP_LOGI(TAG, "Characteristic discovery complete");

        if (target_char_handle != 0) {
            const char *msg = "HEARTBEAT";

            ble_gattc_write_flat(conn_handle,
                                target_char_handle,
                                msg,
                                strlen(msg),
                                gatt_write_cb,
                                NULL);
        } else {
            ESP_LOGE(TAG, "Target characteristic not found!");
        }
    } else {
        ESP_LOGE(TAG, "Characteristic discovery error: %d", error->status);
    }
    return 0;
}

static int gatt_write_cb(uint16_t conn_handle,
                         const struct ble_gatt_error *error,
                         struct ble_gatt_attr *attr,
                         void *arg)
{
    if (error->status == 0) {
        ESP_LOGI(TAG, "Write successful");
    } else {
        ESP_LOGE(TAG, "Write failed");
    }

    // Efter write  så disconnectar edge från mesh-nod
    ble_gap_terminate(conn_handle,
                      BLE_ERR_REM_USER_CONN_TERM);
    
    return 0;
}