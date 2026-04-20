#include "node.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include <time.h>
#include "esp_mac.h"
#include "esp_system.h"

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

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2],
             mac[3], mac[4], mac[5]);

    while(1)
    {
        if (xQueueReceive(ble_queue, &incomming, portMAX_DELAY))
        {
            
            if (!verify_edge_message((uint8_t*)&incomming, sizeof(edge_event_t)))
            {
                ESP_LOGW(TAG, "Auth failed for device %s! Rejecting.", mac_str);
                send_ble_nack(incomming.seq);
                continue;
            }
            ESP_LOGI(TAG, "Processing event from device %s", mac_str);

            memcpy(outgoing.device_id, mac, sizeof(outgoing.device_id));
            outgoing.event_type = incomming.event_type;
            outgoing.battery = incomming.battery_status;
            outgoing.seq = incomming.seq;
            outgoing.location = incomming.event_location;

            time_t now;
            time(&now);
            struct tm timeinfo;
            localtime_r(&now, &timeinfo);

            uint8_t hour = timeinfo.tm_hour;
            uint8_t minute = timeinfo.tm_min;

            outgoing.timestamp = (hour << 6) | minute;
            memset(outgoing.auth_tag, 0, AUTH_TAG_LEN);

            generate_auth_tag((uint8_t*)&outgoing,
                              offsetof(edge_event_out, auth_tag),
                              outgoing.auth_tag
            );

            switch(outgoing.event_type)
            {
                case 0: 
                {
                    ESP_LOGI(TAG, "Heartbeat from device %s (batt: %d%%)",
                             mac_str, outgoing.battery);
                    break;
                }
                case 1:
                {
                    ESP_LOGI(TAG, "Fall detected! device %s @location: %d time of event: %d", 
                        mac_str,
                        outgoing.location,
                        outgoing.timestamp);
                    break;
                }
                case 2:
                {
                    ESP_LOGI(TAG, "Gas detected! device %s @location: %d time of event: %d",
                        mac_str,
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