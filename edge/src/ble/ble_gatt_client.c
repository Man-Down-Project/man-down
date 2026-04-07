#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_uuid.h"
#include "host/ble_hs_adv.h"

#include "ble/ble.h"
#include "ble/ble_internal.h"
#include "ble/ble_nodes.h"
#include "ble/ble_gatt_client.h"
#include "security/provisioning.h"

// Service UUID etc
static ble_uuid_any_t target_service_uuid;
static ble_uuid_any_t event_rx_uuid;
static ble_uuid_any_t ack_tx_uuid;
static ble_uuid_any_t provisioning_service_uuid;
static ble_uuid_any_t provisioning_char_uuid;
static uint16_t provisioning_char_handle = 0;

static const char *TAG = "[BLE_GATT]";
// GATT variables
static uint16_t event_char_handle = 0;
//static uint16_t target_char_handle = 0;
static uint16_t ack_cccd_handle = 0;
static uint16_t service_end_handle = 0; // storing svc callback
static uint16_t service_start_handle = 0;
static uint16_t ack_char_handle = 0;
static uint16_t provisioning_cccd_handle = 0;
static uint16_t cccd_handle = 0;

//test variabler
// static uint16_t normal_svc_start = 0;
// static uint16_t normal_svc_end   = 0;

static uint16_t prov_svc_start   = 0;
static uint16_t prov_svc_end     = 0;


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
static int provisioning_read_cb(uint16_t conn_handle,
                                const struct ble_gatt_error *error,
                                struct ble_gatt_attr *attr,
                                void *arg);

// Funcs
void gatt_set_handles(uint16_t svc_start,
                      uint16_t svc_end,
                      uint16_t event_handle,
                      uint16_t ack_handle,
                      uint16_t cccd_handle)
{
    service_start_handle = svc_start;
    service_end_handle   = svc_end;
    event_char_handle    = event_handle;
    ack_char_handle      = ack_handle;
    ack_cccd_handle      = cccd_handle;

    ESP_LOGI(TAG, "GATT handles applied from cache");
    ble_state = BLE_STATE_SUBSCRIBING;
    gatt_enable_notifications(current_conn_handle, ack_cccd_handle);
}

int gatt_svc_cb(uint16_t conn_handle,
                     const struct ble_gatt_error *error,
                     const struct ble_gatt_svc *service,
                     void *arg)
{
    
    if (error->status == 0) {

        if (ble_uuid_cmp(&service->uuid.u, &provisioning_service_uuid.u) == 0) {
            ESP_LOGI(TAG, "Provisioning service found");

            prov_svc_start = service->start_handle;
            prov_svc_end   = service->end_handle;
        }
        ESP_LOGI(TAG, "Service found: start=%d end=%d", service->start_handle, service->end_handle);
        if (ble_uuid_cmp(&service->uuid.u, &target_service_uuid.u) == 0) {
            service_start_handle = service->start_handle;
            service_end_handle   = service->end_handle;

            int n_idx = find_node_index(&current_peer_addr);
            if (n_idx >= 0) {
                nodes[n_idx].svc_start = service_start_handle;
                nodes[n_idx].svc_end   = service_end_handle;
            }
        } 

    }
    else if (error->status == BLE_HS_EDONE) {
        ESP_LOGI(TAG, "Service discovery complete");
        if (provisioning_is_active()) {
            if (prov_svc_start != 0) {
                ble_gattc_disc_all_chrs(conn_handle,
                                        prov_svc_start,
                                        prov_svc_end,
                                        gatt_chr_cb,
                                        NULL);
            }
        } else {
            if(service_start_handle != 0) {
                ble_gattc_disc_all_chrs(conn_handle,
                                        1, 0xFFFF,
                                        gatt_chr_cb,
                                        NULL);
            }
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
        ESP_LOGI(TAG, "Mode: %s",
                 provisioning_is_active() ? "PROVISIONING" : "NORMAL");
        char uuid_str[BLE_UUID_STR_LEN];
        ble_uuid_to_str(&chr->uuid.u, uuid_str);

        ESP_LOGI(TAG, "Characteristic UUID: %s", uuid_str);
        ESP_LOGI(TAG, "Characteristic found: def_handle=%d val_handle=%d",
                 chr->def_handle,
                 chr->val_handle);
        
        if (provisioning_is_active() &&
            ble_uuid_cmp(&chr->uuid.u, &provisioning_char_uuid.u) == 0) {
                provisioning_char_handle = chr->val_handle; 
                ESP_LOGI(TAG, "Provisioning characteristic found!");

                //provisorisk lösning "Read" tills notify är fixat på fog
                
                // ble_gattc_read(conn_handle,
                //                provisioning_char_handle,
                //                provisioning_read_cb,
                //                NULL);
                // return 0;
            }
        if (ble_uuid_cmp(&chr->uuid.u, &event_rx_uuid.u) == 0) {
            event_char_handle = chr->val_handle;
            ESP_LOGI(TAG, "eventRX found!");

            int node_index = find_node_index(&current_peer_addr);
            if (node_index >= 0) {
                nodes[node_index].tx_handle = event_char_handle;
            }
        }
        if (ble_uuid_cmp(&chr->uuid.u, &ack_tx_uuid.u) == 0) {
            ack_char_handle = chr->val_handle;
            ESP_LOGI(TAG, "ackTX found!");

            int node_index = find_node_index(&current_peer_addr);
            if (node_index >= 0) {
                nodes[node_index].rx_handle = ack_char_handle;
            }
        }
        return 0;
    }
    if (error->status == BLE_HS_EDONE) {
        ESP_LOGI(TAG, "Characteristic discovery complete");

        if (provisioning_is_active()) {
            if (provisioning_char_handle != 0) {
                ble_gattc_disc_all_dscs(conn_handle,
                                        provisioning_char_handle,
                                        0xFFFF,
                                        gatt_dsc_cb,
                                        NULL);
                ble_gattc_read(conn_handle,
                               provisioning_char_handle,
                               provisioning_read_cb,
                               NULL);
                ESP_LOGI(TAG, "Reading provisioning characteristic...");
            } else {
                ESP_LOGE(TAG, "Provisioning characteristic not found!");
            }
        } else {
            if (ack_char_handle != 0) {
                ble_gattc_disc_all_dscs(conn_handle,
                                        ack_char_handle,
                                        0xFFFF,
                                        gatt_dsc_cb,
                                        NULL);
            } else {
                ESP_LOGE(TAG, "ACK characteristic not found!");
            }
        }
        return 0;
    }
    ESP_LOGW(TAG, "Characteristic discovery error: %d", error->status);
    return 0;
}

static int gatt_write_cb(uint16_t conn_handle,
                         const struct ble_gatt_error *error,
                         struct ble_gatt_attr *attr,
                         void *arg)
{
    if (error->status == 0) 
    {
        ESP_LOGI(TAG, "Write successful");
    } 
    else 
    {
        ESP_LOGE(TAG, "Write failed");
    }
    return 0;
}

static int gatt_dsc_cb(uint16_t conn_handle,
                       const struct ble_gatt_error *error,
                       uint16_t chr_val_handle,
                       const struct ble_gatt_dsc *dsc,
                       void *arg)
{
    if (error->status != 0) {
        return 0;
    }
    if (ble_uuid_u16(&dsc->uuid.u) == 0x2902) {
            
        if (provisioning_is_active()) {
            
            provisioning_cccd_handle = dsc->handle;
            ESP_LOGI(TAG, "Provisioning CCCD found -> enabling notify");
            gatt_enable_notifications(conn_handle, provisioning_cccd_handle);
        } else {

        
            ack_cccd_handle = dsc->handle;
            ESP_LOGI(TAG, "ACK CCCD found");

            int n_idx = find_node_index(&current_peer_addr);
            if (n_idx >= 0) {
                nodes[n_idx].cccd_handle = ack_cccd_handle;
            }
            gatt_enable_notifications(conn_handle, ack_cccd_handle);
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
        notifications_ready = true;

        int n_idx = find_node_index(&current_peer_addr);
        if (n_idx >= 0) {
            nodes[n_idx].gatt_cached = true;

            ESP_LOGI(TAG, "GATT cached: node_%d svc[%d-%d] tx=%d rx=%d notify=%d",
                     n_idx + 1,
                     nodes[n_idx].svc_start,
                     nodes[n_idx].svc_end,
                     nodes[n_idx].tx_handle,
                     nodes[n_idx].rx_handle,
                     nodes[n_idx].cccd_handle);
        }
        // Write event packet after subscribing to ackTX
        ble_on_ready(conn_handle);
    } else {
        ESP_LOGE(TAG, "Failed to enable notify");
    }
    
    return 0;
}

static int provisioning_read_cb(uint16_t conn_handle,
                                const struct ble_gatt_error *error,
                                struct ble_gatt_attr *attr,
                                void *arg)
{
    if (error->status != 0) {
        ESP_LOGE(TAG, "Provisioning read failed: %d", error->status);
        return 0;
    }
    uint16_t len = OS_MBUF_PKTLEN(attr->om);
    uint8_t buffer[16];

    ble_hs_mbuf_to_flat(attr->om, buffer, sizeof(buffer), &len);
    ESP_LOGI(TAG, "Provisioning data received via READ");
    provisioning_handle_rx(buffer, len);
    return 0;
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
    
    ble_uuid_from_str(&provisioning_service_uuid,
                      "12345678-1234-1234-1234-1234567890ab");
    
    ble_uuid_from_str(&provisioning_char_uuid,
                      "12345678-1234-1234-1234-1234567890bc");
}

const ble_uuid_t *gatt_get_service_uuid()
{
    return &target_service_uuid.u;
}

int gatt_send_event(uint16_t conn_handle, edge_event_t *event)
{
    
    if (event_char_handle == 0)
    {
        ESP_LOGE(TAG, "Characteristic handle not set");
        return -1;
    }
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE) 
    {
        ESP_LOGE(TAG, "No active connection");
        return -1;
    }
    int rc = ble_gattc_write_flat(conn_handle,
                                  event_char_handle,
                                  event,
                                  sizeof(edge_event_t),
                                  gatt_write_cb,
                                  NULL);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Write failed: %d", rc);
        return rc;
    }
    ESP_LOGI(TAG, "Event sent");
    return 0;
}

void gatt_enable_notifications(uint16_t conn_handle, uint16_t cccd_handle)
{
    if (cccd_handle == 0)
    {
        ESP_LOGE(TAG, "CCCD handle missing!");
        return;
    }
    uint16_t enable = 0x0001;
    ESP_LOGI(TAG, "Re-enabling notifications (conn=%d), cccd=%d", conn_handle, cccd_handle);

    int rc = ble_gattc_write_flat(conn_handle,
                                  cccd_handle,
                                  &enable,
                                  sizeof(enable),
                                  subscribe_cb,
                                  NULL);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Failed to write CCCD: %d", rc);
    }
}

const ble_uuid_t *gatt_get_provisioning_service_uuid(void)
{
    return &provisioning_service_uuid.u;
}