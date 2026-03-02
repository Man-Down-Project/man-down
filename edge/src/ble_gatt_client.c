#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"

#include "ble.h"
#include "edge_protocol.h"

static const char *TAG = "BLE_GATT";
// GATT variables
static uint16_t target_char_handle = 0;
static uint16_t ack_cccd_handle = 0;
static uint16_t service_end_handle; // storing svc callback

static uint8_t sequence_counter = 0;
// Func declarations
static int gatt_write_cb(uint16_t conn_handle,
                         const struct ble_gatt_error *error,
                         struct ble_gatt_attr *attr,
                         void *arg);
static int gatt_chr_cb(uint16_t conn_handle,
                       const struct ble_gatt_error *error,
                       const struct ble_gatt_chr *chr,
                       void *arg);
static int subscribe_cb(uint16_t conn_handle,
                        const struct ble_gatt_error *error,
                        struct ble_gatt_attr *attr,
                        void *arg);
static int gatt_dsc_cb(uint16_t conn_handle,
                       const struct ble_gatt_error *error,
                       uint16_t chr_val_handle,
                       const struct ble_gatt_dsc *dsc,
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
            //const char *msg = "HEARTBEAT"; // just here for testing
            ble_gattc_disc_all_dscs(conn_handle,
                                target_char_handle,
                                service_end_handle,
                                gatt_dsc_cb,
                                NULL);

            // ble_gattc_write_flat(conn_handle,
            //                     target_char_handle,
            //                     msg,
            //                     strlen(msg),
            //                     gatt_write_cb,
            //                     NULL);
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

static int gatt_dsc_cb(uint16_t conn_handle,
                       const struct ble_gatt_error *error,
                       uint16_t chr_val_handle,
                       const struct ble_gatt_dsc *dsc,
                       void *arg)
{
    if (error->status == 0) {
        if (dsc->uuid.u.type == BLE_UUID_TYPE_16 &&
            ble_uuid_u16(&dsc->uuid.u) == 0x2902) {
            // CCCD - Characteristic callback description found
            ack_cccd_handle = dsc->handle;

            uint16_t enable_notify = 0x0001;
            ble_gattc_write_flat(conn_handle,
                                 ack_cccd_handle,
                                 &enable_notify,
                                 sizeof(enable_notify),
                                 subscribe_cb,
                                 NULL);
        }
    }
    return 0;
}

static int subscribe_cb(uint16_t conn_handle,
                        const struct ble_gatt_error *error,
                        struct ble_gatt_attr *attr,
                        void *arg)
{
    if (error->status == 0) {
        ESP_LOGI(TAG, "Notifications enabled");

        // Write event packet after subscribing to ackTX
        // send_edge_packet(conn_handle);
    } else {
        ESP_LOGE(TAG, "Failed to enable notify");
    }
    return 0;
}

void send_edge_packet(uint16_t conn_handle)
{
    if (target_char_handle == 0) {
        ESP_LOGE(TAG, "Characteristic handle not set");
        return;
    }
    edge_event_t packet;

    packet.device_id        = 1;            // example
    packet.event_type       = 0x00;         // heartbeat
    packet.event_location   = 2;            // example location
    packet.battery_status   = 95;           // levels in %
    packet.seq              = sequence_counter++;

    int rc = ble_gattc_write_flat(conn_handle,
                                  target_char_handle,
                                  &packet,
                                  sizeof(packet),
                                  gatt_write_cb,
                                  NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Write failed: %d, rc");
    } else {
        ESP_LOGI(TAG, "Packet sent (seq=%d)", packet.seq);
    }
}