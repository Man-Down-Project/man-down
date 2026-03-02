#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_uuid.h"

#include "ble.h"
#include "edge_protocol.h"

static ble_uuid_any_t target_service_uuid;
static ble_uuid_any_t event_rx_uuid;
static ble_uuid_any_t ack_tx_uuid;

static const char *TAG = "BLE_GATT";
// GATT variables
static uint16_t event_char_handle = 0;
//static uint16_t target_char_handle = 0;
static uint16_t ack_cccd_handle = 0;
static uint16_t service_end_handle = 0; // storing svc callback
static uint16_t service_start_handle = 0;
static uint16_t ack_char_handle = 0;

static uint8_t sequence_counter = 0;
// Func declarations
void send_edge_packet(uint16_t conn_handle);
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
    // service_start_handle = 0;
    // service_end_handle   = 0;
    // event_char_handle    = 0;
    // ack_char_handle      = 0;
    // ack_cccd_handle      = 0;

    if (error->status == 0) {

        ESP_LOGI(TAG, "Service found: start=%d end=%d", service->start_handle, service->end_handle);
        if (ble_uuid_cmp(&service->uuid.u, &target_service_uuid.u) == 0) {
            service_start_handle = service->start_handle;
            service_end_handle   = service->end_handle;
        } 

    }
    else if (error->status == BLE_HS_EDONE) {
        ESP_LOGI(TAG, "Service discovery complete");
        if (service_start_handle != 0) {
            ble_gattc_disc_all_chrs(conn_handle,
                                    service_start_handle,
                                    service_end_handle,
                                    gatt_chr_cb,
                                    NULL);
        }
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

        char uuid_str[BLE_UUID_STR_LEN];
        ble_uuid_to_str(&chr->uuid.u, uuid_str);
        ESP_LOGI(TAG, "Characteristic UUID: %s", uuid_str);

        ESP_LOGI(TAG, "Characteristic found: def_handle=%d val_handle=%d",
                 chr->def_handle,
                 chr->val_handle);
        
        // Check for eventRX (write characteristics)
        if (ble_uuid_cmp(&chr->uuid.u, &event_rx_uuid.u) == 0) {
            event_char_handle = chr->val_handle;
            ESP_LOGI(TAG, "eventRX found!");
        }

        // Check for ackTX (notify characteristic)
        if (ble_uuid_cmp(&chr->uuid.u, &ack_tx_uuid.u) == 0) {
            ack_char_handle = chr->val_handle;
            ESP_LOGI(TAG, "ackTX found!");
        }
    }
    else if (error->status == BLE_HS_EDONE) {
        
        ESP_LOGI(TAG, "Characteristic discovery complete");

        if (ack_char_handle != 0) {
            //const char *msg = "HEARTBEAT"; // just here for testing
            ble_gattc_disc_all_dscs(conn_handle,
                                ack_char_handle,
                                service_end_handle,
                                gatt_dsc_cb,
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
        send_edge_packet(conn_handle);
    } else {
        ESP_LOGE(TAG, "Failed to enable notify");
    }
    return 0;
}

void send_edge_packet(uint16_t conn_handle)
{
    if (event_char_handle == 0) {
        ESP_LOGE(TAG, "Characteristic handle not set");
        return;
    }
    edge_event_t packet;

    packet.device_id        = 137;            // example
    packet.event_type       = 0x00;         // heartbeat
    packet.event_location   = 2;            // example location
    packet.battery_status   = 95;           // levels in %
    packet.seq              = 1 + sequence_counter++;

    int rc = ble_gattc_write_flat(conn_handle,
                                  event_char_handle,
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

void gatt_client_reset()
{
    service_start_handle = 0;
    service_end_handle   = 0;
    event_char_handle    = 0;
    ack_char_handle      = 0;
    ack_cccd_handle      = 0;
}
void gatt_client_init()
{
    ble_uuid_from_str(&target_service_uuid,
                      "12345678-1234-1234-1234-123456789abc");

    ble_uuid_from_str(&event_rx_uuid,
                      "87654321-4321-4321-4321-cba987654321");

    ble_uuid_from_str(&ack_tx_uuid,
                      "11111111-2222-3333-4444-555555555555");
}

const ble_uuid_t *gatt_get_service_uuid()
{
    return &target_service_uuid.u;
}