#include "node.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include <time.h>

#include "ble.h"
#include "wifi.h"
#include "../config/user_config.h"

extern esp_mqtt_client_handle_t mqtt_client;
static const char *TAG = "[NODE]";

extern QueueHandle_t ble_queue;

static void node_task(void*arg)
{
    edge_event_t incomming;
    edge_event_out outgoing;

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
            ESP_LOGI(TAG, "Processing event from device %d", incomming.device_id);

            outgoing.device_id = incomming.device_id;
            outgoing.event_type = incomming.event_type;
            outgoing.battery = incomming.battery_status;
            outgoing.seq = incomming.seq;
            outgoing.location = incomming.event_location;

            time_t now;
            time(&now);
            outgoing.timestamp = (uint16_t)(now & 0xFFFF);

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

            if (wifi_connected_globally && mqtt_client != NULL) 
            {
                int msg_id = esp_mqtt_client_publish(mqtt_client,
                                                     PUB_TOPIC,
                                                     (const char *)&outgoing,
                                                     sizeof(edge_event_out),
                                                     1, 0);
                if (msg_id != -1) {
                    ESP_LOGI(TAG, "Payload sent to Fog, ID: %d", msg_id);
                }
            }
        }
    }
}

void node_init(void)
{
    xTaskCreate(node_task,
                "node_task",
                8192,
                NULL,
                5,
                NULL);
}