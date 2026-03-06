#include "ble_internal.h"
#include "ble_tx.h"
#include "ble_gatt_client.h"
#include "auth.h"




static edge_event_t tx_packet;

static uint32_t tx_send_time = 0;
static uint32_t last_tx_time = 0;
static uint8_t sequence_counter = 0;

bool tx_packet_pending = false;
volatile bool gatt_busy = false;
QueueHandle_t ble_tx_queue;
TimerHandle_t heartbeat_timer;



static const char *TAG = "TX";


void ble_send_event(const edge_event_t *event)
{
    ble_tx_msg_t msg;
    msg.event = *event;

    if (xQueueSend(ble_tx_queue, &msg, 0) != pdTRUE)
    {
        ESP_LOGW(TAG, "TX queue full");
    }
}

void ble_tx_task(void *arg)
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

void heartbeat_timer_cb(TimerHandle_t xTimer)
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
