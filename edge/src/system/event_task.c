#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "system_events.h"
#include "peripherals/buzzer.h"
#include "peripherals/led.h"
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
            
            led_set(RGB_RED, LED_MODE_BLINK);
            buzzer_play(BUZZER_FALL);
            edge_trigger_event(EVENT_FALLARM, 29);
            break;
        
        case EVENT_BUTTON_LONG:
            
            buzzer_play(BUZZER_GAS);
            edge_trigger_event(EVENT_GASLARM, 90);
            break;
        
        case EVENT_BUTTON_RESET:

            led_set(RGB_OFF, LED_MODE_OFF);
            break;
        
        case EVENT_BUTTON_DOUBLE:

            led_set(RGB_MAGENTA, LED_MODE_BLINK);
            break;
        
        case EVENT_FALL_ALARM:

            led_set(RGB_RED, LED_MODE_SOLID);
            buzzer_play(BUZZER_FALL);
            edge_trigger_event(EVENT_FALLARM, 75);
            break;
        
        default:
            break;
    }
}

static void event_task(void *arg)
{
    system_event_t ev;

    while(1)
    {
        if (xQueueReceive(event_queue, &ev, portMAX_DELAY))
        {
            handle_event(&ev);
        }
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