#include <string.h>
#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "nimble/hci_common.h"
#include "nvs_flash.h"

#include "wifi_ble/ble.h"
#include "events/node.h"

static const char *TAG = "[BLE]";
QueueHandle_t ble_queue;
static uint16_t tx_handle;
static uint16_t active_conn_handle = 0xFFFF;
// Service: 12345678-1234-1234-1234-123456789abc
static const ble_uuid128_t service_uuid =
    BLE_UUID128_INIT(
        0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12, 0x34, 0x12, 
        0x34, 0x12, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12
    );

// RX Char: 87654321-4321-4321-4321-cba987654321
static const ble_uuid128_t rx_uuid =
    BLE_UUID128_INIT(
        0x21, 0x43, 0x65, 0x87, 0xa9, 0xcb, 0x21, 0x43, 
        0x21, 0x43, 0x21, 0x43, 0x21, 0x43, 0x65, 0x87
    );

// TX Char: 11111111-2222-3333-4444-555555555555
static const ble_uuid128_t tx_uuid =
    BLE_UUID128_INIT(
        0x55, 0x55, 0x55, 0x55, 0x44, 0x44, 0x33, 0x33, 
        0x22, 0x22, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11
    );

static int ble_rx_cb(uint16_t conn_handle,
                     uint16_t attr_handle,
                     struct ble_gatt_access_ctxt *ctxt,
                     void *arg);
static int ble_gap_event(struct ble_gap_event *event, void *arg);

// 1. Define the Characteristics separately
static const struct ble_gatt_chr_def mesh_chrs[] = {
    {
        // RX Characteristic
        .uuid = BLE_UUID128_DECLARE(
            0x21, 0x43, 0x65, 0x87, 0xa9, 0xcb, 0x21, 0x43, 
            0x21, 0x43, 0x21, 0x43, 0x21, 0x43, 0x65, 0x87),
        .access_cb = ble_rx_cb,
        .flags = BLE_GATT_CHR_F_WRITE,
    },
    {
        // TX Characteristic
        .uuid = BLE_UUID128_DECLARE(
            0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x44, 0x44,
            0x33, 0x33, 0x22, 0x22, 0x11, 0x11, 0x11, 0x11),
        .access_cb = ble_rx_cb,
        .flags = BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &tx_handle,
    },
    {0} // Terminator for characteristics
};

// 2. Define the Service
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID128_DECLARE(0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12, 0x34, 0x12, 
                                    0x34, 0x12, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12),
        .characteristics = mesh_chrs,
    },
    {0} // Terminator for services
};

static int ble_rx_cb(uint16_t conn_handle,
                     uint16_t attr_handle,
                     struct ble_gatt_access_ctxt *ctxt,
                     void *arg)
{
    ESP_LOGI(TAG, "Data received over BLE");
   
    uint16_t len = ctxt->om->om_len;
    edge_event_t event;
    
    if (len != sizeof(edge_event_t))
    {
        ESP_LOGE(TAG, "Invalid packet size: %d (expected %d)",
                 len, sizeof(edge_event_t));
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    
    memcpy(&event, ctxt->om->om_data, sizeof(edge_event_t));
    node_ack_t ack_packet;
    ack_packet.seq = event.seq;
    ack_packet.status = 0;
    
    struct os_mbuf *om = ble_hs_mbuf_from_flat(&ack_packet, sizeof(ack_packet));
    if (om && tx_handle != 0)
    {
        int rc = ble_gatts_notify_custom(conn_handle, tx_handle, om);
        if (rc == 0) {
            ESP_LOGI(TAG, "ACK sent: seq=%d, status=%d", ack_packet.seq, ack_packet.status);
        } else {
            ESP_LOGE(TAG, "Failed to send ACK; rc=%d", rc);
        }
    } else {
        ESP_LOGE(TAG, "Failed to allocate mbuf for ACK");
    }
    ESP_LOGI(TAG, "BLE RX: dev=%d, type=%d, loc=%d, batt=%d seq=%d",
             event.device_id,
             event.event_type,
             event.event_location,
             event.battery_status,
             event.seq);

    if (xQueueSend(ble_queue, &event, 0) != pdTRUE)
    {
        ESP_LOGW(TAG, "Queue full, dropping event");
    }
    return 0;
}

static void ble_app_advertise(void)
{
    struct ble_gap_adv_params adv_params = {0};

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    ble_gap_adv_start(0, 
                      NULL, 
                      BLE_HS_FOREVER, 
                      &adv_params, 
                      ble_gap_event, 
                      NULL);
}

static void ble_on_sync(void)
{
    int rc;

    // Set address auto
    rc = ble_hs_id_infer_auto(0, (uint8_t[6]){0});
    assert(rc == 0);

    // 1. MAIN ADVERTISEMENT DATA (Flags + UUID)
    struct ble_hs_adv_fields adv_fields = {0};
    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    
    // Put the 128-bit UUID here (18 bytes total)
    adv_fields.uuids128 = (ble_uuid128_t *)&service_uuid;
    adv_fields.num_uuids128 = 1;
    adv_fields.uuids128_is_complete = 1;

    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Main adv failed; rc=%d", rc);
        return;
    }
    
    // 2. SCAN RESPONSE DATA (Device Name)
    struct ble_hs_adv_fields scan_fields = {0};
    scan_fields.name = (uint8_t *)"MESH_NODE";
    scan_fields.name_len = 9;
    scan_fields.name_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&scan_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Scan response failed; rc=%d", rc);
        return;
    }
    adv_fields.adv_itvl = 0x0010;
    adv_fields.adv_itvl_long = 0x0020;
    adv_fields.adv_itvl_is_present = 1;
    adv_fields.adv_itvl_long_is_present = 1;

    

    // 3. START
    ble_app_advertise();
}

void ble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}
void ble_init(void)
{
    int rc;

    // Create queue before starting BLE
    ble_queue = xQueueCreate(10, sizeof(edge_event_t));
    if (ble_queue == NULL) {
        ESP_LOGE("BLE", "Failed to create queue!");
        return; 
    }

    ESP_LOGI(TAG, "initializing NimBLE port");
    rc = nimble_port_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "nimble_port_init failed; rc=%d", rc);
        return;
    }

    /* Initialize the NimBLE host configuration. */
    ble_hs_cfg.sync_cb = ble_on_sync;
    
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_bonding = 0;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 0;

    // Initialize services
    ble_svc_gap_init();
    ble_svc_gatt_init();

    // --- CHECK THESE TWO SPECIFICALLY ---
    rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed; rc=%d", rc);
        return;
    }

    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed; rc=%d", rc);
        return;
    }
    // ------------------------------------

    ESP_LOGI(TAG, "Starting NimBLE host task");
    nimble_port_freertos_init(ble_host_task);
}

static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch(event->type)
    {
        case BLE_GAP_EVENT_CONNECT:
        {
            if (event->connect.status == 0) {
                ESP_LOGI(TAG, "Connection established");
                active_conn_handle = event->connect.conn_handle;
            } else {
                ESP_LOGE(TAG, "Connection failed; status=%d", event->connect.status);
                ble_app_advertise();
            }

            break;
        }
        case BLE_GAP_EVENT_DISCONNECT:
        {
            ESP_LOGI(TAG, "Disconnected; reason=%d", event->disconnect.reason);
            ble_app_advertise();
            active_conn_handle = 0xFFFF;
            break;
        }
        case BLE_GAP_EVENT_CONN_UPDATE:
        {
            ESP_LOGI(TAG, "Connection updated; status=%d", event->conn_update.status);
            break;
        }
        case BLE_GAP_EVENT_MTU:
        {
            ESP_LOGI(TAG, "MTU updated to %d", event->mtu.value);
            break;
        }
        default:
        {
            break;
        }
    }
    return 0;
}
void nvs_init(void)
{
    ESP_LOGI(TAG, "Initializing NVS");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void send_ble_nack(uint8_t seq)
{
    if (active_conn_handle == 0xFFFF || tx_handle == 0) return;

    node_ack_t nack;
    nack.seq = seq;
    nack.status = CMD_AUTH_FAIL;

    struct os_mbuf *om = ble_hs_mbuf_from_flat(&nack, sizeof(nack));
    ble_gatts_notify_custom(active_conn_handle, tx_handle, om);
    ble_gap_terminate(active_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}