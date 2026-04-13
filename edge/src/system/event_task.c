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
#include "peripherals/buzzer.h"
#include "peripherals/led.h"
#include "peripherals/onboard_led.h"
#include "config/edge_config.h"
#include "event/edge_event.h"

static QueueHandle_t event_queue;

void system_event_post(system_event_type_t type, uint32_t data)
{
    system_event_t ev = {
        .type = type,
        .data = data
    };

    xQueueSend(event_queue, &ev, 0);
}

static void handle_event(system_event_t *ev)
{
    switch(ev->type)
    {
        case EVENT_BUTTON_SHORT:
            
            led_set(RGB_RED, LED_MODE_BLINK, LED_PRIO_HIGH);
            onboard_led_set(RGB_RED, LED_MODE_BLINK, LED_PRIO_HIGH);
            buzzer_play(BUZZER_FALL);
            edge_trigger_event(EVENT_FALLARM, 29);
            break;
        
        case EVENT_BUTTON_LONG:
            led_set(RGB_BLUE, LED_MODE_BLINK, LED_PRIO_HIGH);
            onboard_led_set(RGB_DARK_PURPLE, LED_MODE_SOLID, LED_PRIO_HIGH);
            buzzer_play(BUZZER_GAS);
            edge_trigger_event(EVENT_GASLARM, 90);
            break;
        
        case EVENT_BUTTON_RESET:
            
            buzzer_stop();
            led_off();onboard_led_off();
            led_set(RGB_YELLOW, LED_MODE_BLINK, LED_PRIO_HIGH);
            onboard_led_set(RGB_YELLOW, LED_MODE_BLINK, LED_PRIO_HIGH);
            vTaskDelay(pdMS_TO_TICKS(500));            
            led_off();onboard_led_off();
            break;
        
        case EVENT_BUTTON_DOUBLE:

            led_set(RGB_MAGENTA, LED_MODE_BLINK, LED_PRIO_HIGH);
            onboard_led_set(RGB_MAGENTA, LED_MODE_BLINK, LED_PRIO_HIGH);
            vTaskDelay(pdMS_TO_TICKS(500));
            nvs_flash_erase();
            esp_restart();
            break;
        
        case EVENT_FALL_ALARM:

            led_set(RGB_RED, LED_MODE_SOLID, LED_PRIO_HIGH);
            onboard_led_set(RGB_RED, LED_MODE_SOLID, LED_PRIO_HIGH);
            buzzer_play(BUZZER_FALL);
            edge_trigger_event(EVENT_FALLARM, 75);
            break;
            
        case EVENT_BUTTON_POWER:
            led_set(RGB_YELLOW, LED_MODE_BLINK, LED_PRIO_HIGH);
            onboard_led_set(RGB_YELLOW, LED_MODE_BLINK, LED_PRIO_HIGH);
            vTaskDelay(pdMS_TO_TICKS(1000));
            onboard_led_set(RGB_RED, LED_MODE_SOLID, LED_PRIO_HIGH);
            led_set(RGB_RED, LED_MODE_SOLID, LED_PRIO_HIGH);
            vTaskDelay(pdMS_TO_TICKS(1000));
            led_off();
            onboard_led_off();
            nvs_flash_erase();
            esp_deep_sleep_start();
            break;

        case EVENT_GAS_ALARM:
            led_set(RGB_YELLOW, LED_MODE_BLINK, LED_PRIO_HIGH);
            onboard_led_set(RGB_YELLOW, LED_MODE_SOLID, LED_PRIO_HIGH);
            buzzer_play(BUZZER_GAS);
            edge_trigger_event(EVENT_GASLARM, 95);
            break;

        // case EVENT_DEBUG:
        //     led_set(RGB_YELLOW, LED_MODE_SOLID, LED_PRIO_HIGH);
        //     led_set(RGB_YELLOW, LED_MODE_SOLID, LED_PRIO_HIGH);


        default:
            break;
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