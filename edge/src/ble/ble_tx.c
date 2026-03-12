#include "esp_event.h"

#include "config/edge_config.h"
#include "ble/ble_internal.h"
#include "ble/ble_tx.h"
#include "ble/ble_gatt_client.h"
#include "security/auth.h"
#include "event/edge_event.h"
#include "peripherals/battery.h"




static edge_event_t tx_packet;

static int retry_count = 0;
static uint32_t tx_send_time = 0;
static uint32_t last_tx_time = 0;
uint8_t sequence_counter = 0;

bool tx_packet_pending = false;
volatile bool gatt_busy = false;
QueueHandle_t ble_tx_queue;
TimerHandle_t heartbeat_timer;

static const char *TAG = "[BLE_TX]";

void ble_ack_received(uint8_t seq, uint8_t status)
{
    if (!tx_packet_pending)
    {
        return;
    }
    if (seq == tx_packet.seq)
    {
        ESP_LOGI(TAG, "ACK matched seq=%d status=%d", seq, status);
        tx_packet_pending = false;
        gatt_busy = false;
    }
    else
    {
        ESP_LOGW(TAG, "ACK for old seq=%d (current=%d)", seq, tx_packet.seq);
    }
}

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

                retry_count = 0;

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
            ESP_LOGI(TAG, "TX sending seq=%d (retry=%d)", tx_packet.seq, retry_count);

            gatt_busy = true;

            last_tx_time = xTaskGetTickCount();
            tx_send_time = last_tx_time;
            int rc = gatt_send_event(current_conn_handle, &tx_packet);

            if (rc != 0)
            {
                gatt_busy = false;
                ESP_LOGW(TAG, "TX failed, will retry");
            }
        }
        if (ack_timeout)
        {
            retry_count++;

            if(retry_count > MAX_RETRIES)
            {
                ESP_LOGW(TAG, "Max retries reached seq=%d", tx_packet.seq);
                
                tx_packet_pending = false;
                gatt_busy = false;
                retry_count = 0;
            }
            else
            {
                ESP_LOGW(TAG, "ACK timeout, retry %d seq=%d", retry_count, tx_packet.seq);
                gatt_busy = false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void heartbeat_timer_cb(TimerHandle_t xTimer)
{
    battery_set(99);
    edge_trigger_event(EVENT_HEARTBEAT, battery_get());
}
