#include "node.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include <time.h>

#include "wifi_ble/ble.h"
#include "wifi_ble/wifi.h"
#include "config/user_config.h"
#include "security/auth.h"
#include "event_task.h"

edge_event_out outgoing;

extern esp_mqtt_client_handle_t mqtt_client;
static const char *TAG = "[NODE]";

extern QueueHandle_t ble_queue;

static void node_task(void*arg)
{
    edge_event_t incomming;
    

    if (ble_queue == NULL)
    {
        ESP_LOGE(TAG, "BLE queue not initialized!");
        vTaskDelete(NULL);
        return;
    }
    while(1)
    {
        if (xQueueReceive(ble_queue, &incomming, portMAX_DELAY))
        {
            
            if (!verify_edge_message((uint8_t*)&incomming, sizeof(edge_event_t)))
            {
                ESP_LOGW(TAG, "Auth failed for device %d! Rejecting.", incomming.device_id);
                send_ble_nack(incomming.seq);
                continue;
            }
            ESP_LOGI(TAG, "Processing event from device %d", incomming.device_id);

            outgoing.device_id = incomming.device_id;
            outgoing.event_type = incomming.event_type;
            outgoing.battery = incomming.battery_status;
            outgoing.seq = incomming.seq;
            outgoing.location = incomming.event_location;

            time_t now;
            time(&now);
            outgoing.timestamp = (uint16_t)(now & 0xFFFF);
            memset(outgoing.auth_tag, 0, AUTH_TAG_LEN);

            generate_auth_tag((uint8_t*)&outgoing,
                              offsetof(edge_event_out, auth_tag),
                              outgoing.auth_tag
            );

            switch(outgoing.event_type)
            {
                case 0: 
                {
                    ESP_LOGI(TAG, "Heartbeat from device %d (batt: %d%%)",
                             outgoing.device_id, outgoing.battery);
                    break;
                }
                case 1:
                {
                    ESP_LOGI(TAG, "Fall detected! device %d @location: %d time of event: %d", 
                        outgoing.device_id,
                        outgoing.location,
                        outgoing.timestamp);
                    break;
                }
                case 2:
                {
                    ESP_LOGI(TAG, "Gas detected! device %d @location: %d time of event: %d",
                        outgoing.device_id,
                        outgoing.location,
                        outgoing.timestamp);
                    break;;
                }
                default:
                {
                    ESP_LOGW(TAG, "Unknown event type: %d", outgoing.event_type);
                    break;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(50));
            system_event_post(EVENT_HEARTBEAT, 0);
            
        }
    }
}

void node_init(void)
{
    xTaskCreate(node_task,
                "node_task",
                10240,
                NULL,
                5,
                NULL);
}