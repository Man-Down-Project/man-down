#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

#include "esp_log.h"
#include "esp_bt.h"
#include "nvs_flash.h"

#include "host/ble_hs.h"
#include "host/ble_gap.h"

#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "config/edge_config.h"
#include "ble/ble_internal.h"
#include "ble/ble_nodes.h"
#include "ble/ble_gatt_client.h"
#include "ble/ble.h"
#include "ble/ble_tx.h"
#include "ble/ble_gap.h"
#include "security/provisioning.h"
//#include "system/system_events.h"

// --------------------------------------------------------------------------
// >                     BLE Scan Parameters                                <
// --------------------------------------------------------------------------

static struct ble_gap_disc_params scan_params = {
        .itvl = 0x40,
        .window = 0x30,
        .filter_policy = 0,
        .passive = 0,
        .limited = 0,
        .filter_duplicates = 1
};

static int gap_event_connect(struct ble_gap_event *event);
static int gap_event_disconnect(struct ble_gap_event *event);
static int gap_event_disc(struct ble_gap_event *event);
static int gap_event_disc_complete(struct ble_gap_event *event);
static int gap_event_enc_change(struct ble_gap_event *event);
static int gap_event_notify(struct ble_gap_event *event);

static uint32_t last_roam_time = 0;
TimerHandle_t pairing_timer;
static bool pairing_failed = false;
static uint32_t pairing_start_time = 0;

static const char *TAG = "[BLE_GAP]";

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
    notifications_ready = false;

    if (provisioning_is_active()) {
        provisioning_on_connected(event->connect.conn_handle);

        ESP_LOGI(TAG, "Starting secure pairing for provisioning");
        // int rc = ble_gap_security_initiate(event->connect.conn_handle);
        // if (rc != 0) {
        //     ESP_LOGE(TAG, "Security initiation failed: %d", rc);
        // }
    }        
    if (event->connect.status == 0) 
    {
        ble_state = BLE_STATE_CONNECTED;
        pairing_failed = false;
        ESP_LOGI(TAG, "Connected");
        
        current_conn_handle = event->connect.conn_handle;
            
        ble_gap_conn_find(event->connect.conn_handle, &desc);
        ESP_LOGI(TAG, "Bonded: %d", desc.sec_state.bonded);
        
        pairing_start_time = 0;
        current_conn_rssi = -127;
        last_connect_index = -1;
            
        memcpy(&current_peer_addr,
               &desc.peer_ota_addr,
               sizeof(ble_addr_t));
        
        
        int node_index = find_node_index(&current_peer_addr);
        if (node_index >= 0)
        {
            nodes[node_index].fail_count = 0;
            nodes[node_index].blacklist_timer = 0;    
        }
        
        ble_state = BLE_STATE_DISCOVERING;
        ESP_LOGI(TAG, "Encryption established");
        
        if (node_index >= 0 &&
            nodes[node_index].gatt_cached &&
            nodes[node_index].tx_handle != 0)
        {
            ESP_LOGI(TAG, "Using cached GATT handles");

            gatt_set_handles(
                nodes[node_index].svc_start,
                nodes[node_index].svc_end,
                nodes[node_index].tx_handle,
                nodes[node_index].rx_handle,
                nodes[node_index].cccd_handle
            );
            ble_state = BLE_STATE_SUBSCRIBING;   
        }
        else
        {
            ESP_LOGI(TAG, "Running GATT discovery");
            ble_state = BLE_STATE_DISCOVERING;
            ble_gattc_disc_all_svcs(current_conn_handle,
                                    gatt_svc_cb,
                                    NULL);
        }
        
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
    tx_packet_pending = false;
    waiting_for_ack = false;
    gatt_busy = false;
    notifications_ready = false;

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
    
    if (!provisioning_is_active()) {
        goto normal_flow;
    }
    if (desc->length_data == 0 || desc->data == NULL)
        return 0;
    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));

    int rc = ble_hs_adv_parse_fields(&fields, desc->data, desc->length_data);
    if (rc != 0)
        return 0;

    const ble_uuid_t *prov_uuid = gatt_get_provisioning_service_uuid();
    
    if (fields.num_uuids128 > 0) {
        for (int i = 0; i < fields.num_uuids128; i++) {
            if (ble_uuid_cmp(&fields.uuids128[i].u, prov_uuid) == 0) {
                ESP_LOGI(TAG, "Found provisioning device");
                ble_gap_disc_cancel();
                provisioning_on_scan_match(&desc->addr);
                return 0;
            }
        }
    }
    return 0;

normal_flow:
    
    if (desc->rssi < -120 || desc->rssi > 20)
        return 0;
    
    rc = ble_hs_adv_parse_fields(&fields, desc->data, desc->length_data);

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
    
    int node_index = find_node_index(&desc->addr);

    if (node_index >= 0)
    {
        if (!nodes[node_index].valid)
        {
            nodes[node_index].rssi = desc->rssi;
        }
        else
        {
            nodes[node_index].rssi = 
                (nodes[node_index].rssi * (RSSI_SMOOTH_FACTOR -1) + desc->rssi) 
                / RSSI_SMOOTH_FACTOR;
        }
        nodes[node_index].last_seen = xTaskGetTickCount();
                
        if (memcmp(&desc->addr, &current_peer_addr, sizeof(ble_addr_t)) == 0)
        {
            current_conn_rssi = nodes[node_index].rssi;
        }
        uint32_t now = xTaskGetTickCount();

        bool same_node =
            ble_addr_cmp(&nodes[node_index].addr, &current_peer_addr) == 0;
        
        bool strong_node =
            nodes[node_index].rssi > current_conn_rssi + ROAM_THRESHOLD;

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
                     node_index,
                     get_node_id(&nodes[node_index].addr),
                     nodes[node_index].rssi,
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
        nodes[free_slot].gatt_cached = false;
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
        nodes[weakest].gatt_cached = false;
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
        
    if (best_index >= 0 && ble_tx_pending())
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

            ble_gap_disc_cancel();

            int rc = ble_gap_connect(own_addr_type,
                                     &nodes[best_index].addr,
                                     5000,
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
    
    int status = event->enc_change.status;
    ESP_LOGI(TAG, "Encryption status=%d", status);
    gatt_client_reset();
    xTimerStop(pairing_timer, 0);
    pairing_start_time = 0;    
   
    if (status == 0)
    {
        ESP_LOGI(TAG, "Encryption established!");
        if (provisioning_is_active())
        {
            ESP_LOGI(TAG, "Secure provisioning established");
        }
        return 0;
    }
    ESP_LOGE(TAG, "Encryption failed, status=%d", status);

    if (provisioning_is_active())
    {
        int node_index = find_node_index(&current_peer_addr);
        if (!pairing_failed && node_index >= 0)
        {
            nodes[node_index].fail_count++;

            if (nodes[node_index].fail_count >= MAX_CONNECT_FAILS)
            {
                nodes[node_index].blacklist_timer = 
                    xTaskGetTickCount() + NODE_BLACKLIST_TIME;
                
                nodes[node_index].fail_count = 0;

                ESP_LOGW(TAG, "[Node_%d|%02X] Blacklisted: pairing failure", 
                         node_index + 1,
                         get_node_id(&nodes[node_index].addr));
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
    else
    {
        ESP_LOGW(TAG, "Encryption failed, (Connected to mesh node)");
    }
    return 0;
} 

static int gap_event_notify(struct ble_gap_event *event)
{
    struct os_mbuf *om = event->notify_rx.om;

    uint16_t len = OS_MBUF_PKTLEN(om);
    uint8_t buffer[128];

    if (len > sizeof(buffer)) {
        ESP_LOGW(TAG, "Notify too large (%d)", len);
        return 0;
    }
    int rc = ble_hs_mbuf_to_flat(om, buffer, sizeof(buffer), &len);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to flatten mbuf (%d)", rc);
        return 0;
    }
    if (provisioning_is_active()) {
        provisioning_handle_rx(buffer, len);
        return 0;
    }

    if (len < sizeof(edge_ack_t))
    {
        ESP_LOGW(TAG, "Invalid ACK lenght %d", len);
        return 0;
    }
    edge_ack_t ack;
    memcpy(&ack, buffer, sizeof(edge_ack_t));

    ESP_LOGI(TAG, "ACK seq=%d status =%d", ack.seq, ack.status);
    
    ble_ack_received(ack.seq, ack.status);

    return 0;
}

void start_scan(void)
{
    if(ble_state == BLE_STATE_CONNECTING ||
       ble_state == BLE_STATE_CONNECTED ||
       current_conn_handle != BLE_HS_CONN_HANDLE_NONE)
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

void pairing_timeout_cb(TimerHandle_t xTimer)
{
    ESP_LOGW(TAG, "Pairing timeout");

    int node_index = find_node_index(&current_peer_addr);
    if (node_index >= 0)
    {   
        pairing_failed = true;
        node_failure_tracker(node_index);
    }

    if (current_conn_handle != BLE_HS_CONN_HANDLE_NONE)
    {
        ble_gap_terminate(current_conn_handle,
                          BLE_ERR_REM_USER_CONN_TERM);
    }
    pairing_start_time = 0;
}

bool ble_tx_pending(void)
{
    return uxQueueMessagesWaiting(ble_tx_queue) > 0;
}

void ble_connect(void)
{
    int node_index = get_best_node_index();
        if (node_index >= 0)
        {
            ESP_LOGI(TAG, "Connecting to node-%d", node_index);
            if (ble_gap_disc_active())
            {
                ble_gap_disc_cancel();
            }

            int rc = ble_gap_connect(own_addr_type,
                                     &nodes[node_index].addr,
                                     5000,
                                     NULL,
                                     gap_event,
                                     NULL);
            if (rc == 0)
            {
                ble_state = BLE_STATE_CONNECTING;
                return;
            }
        }
        start_scan();
}

void ble_gap_connect_to(const ble_addr_t *addr)
{
    ble_gap_connect(
        own_addr_type,
        addr,
        30000,
        NULL,
        gap_event,
        NULL
    );
}