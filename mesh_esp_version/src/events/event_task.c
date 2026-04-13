#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_task_wdt.h"

#include "system_events.h"
#include "peripherals/onboard_led.h"
#include "events/node.h"
#include "event_task.h"
#include "config/user_config.h"

static QueueHandle_t event_queue;
static const char *TAG = "[HANDLER]";
static int send_event_over_mqtt();

void system_event_post(system_event_type_t type, uint32_t data)
{
    system_event_t ev = {
        .type = type,
        .data = data
    };

    if (event_queue != NULL)
    {
        xQueueSend(event_queue, &ev, 0);
    } else {
        ESP_LOGE(TAG, "event queue not initialized!");
    }
}

static void handle_event(system_event_t *ev)
{
    switch(ev->type)
    {
        
        case EVENT_HEARTBEAT:
        {
            onboard_led_set(RGB_GREEN, LED_MODE_PULSE, LED_PRIO_LOW);
            send_event_over_mqtt();
            break;
        }
        
        default:
        {
            break;
        }
    }
}

static void event_task(void *arg)
{
    system_event_t ev;
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    while(1)
    {
        if (xQueueReceive(event_queue, &ev, pdMS_TO_TICKS(1000)))
        {
            handle_event(&ev);
        }
        esp_task_wdt_reset();
    }
}

void event_task_init()
{
    event_queue = xQueueCreate(10, sizeof(system_event_t));

    xTaskCreate(event_task,
                "event_task",
                4096,
                NULL,
                6,
                NULL);
}

static int send_event_over_mqtt()
{
    if (!wifi_connected_globally || !mqtt_connected) 
    {
        ESP_LOGW(TAG, "MQTT not ready");
        return -1;
    }
        int msg_id = esp_mqtt_client_publish(mqtt_client,
                                             PUB_TOPIC,
                                             (const char *)&outgoing,
                                             sizeof(edge_event_out),
                                             1, 0);
        if (msg_id != -1) 
        {
            ESP_LOGI(TAG, "Payload sent to Fog, ID: %d", msg_id);
        }
        return msg_id;
    
}