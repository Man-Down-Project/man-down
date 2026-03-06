#include <stdbool.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "button.h"
#include "ble/ble.h"
#include "config/edge_config.h"
#include "battery.h"

#define BUTTON_GPIO GPIO_NUM_4
#define DOUBLE_PRESS_MS 400
#define LONG_PRESS_MS 1000
#define LONG_RESET_MS 10000
#define LONG_POWER_MS 20000
#define DEBOUNCE_MS 50

static bool waiting_double_press = false;
static TickType_t first_press_time = 0;

static const char *TAG = "BUTTON";

static button_event_t button_update()
{
    static bool stable_state = false;
    static bool last_press = false;
    static TickType_t last_change = 0;
    static TickType_t press_start = 0;

    bool press = gpio_get_level(BUTTON_GPIO) == 0;
    TickType_t now = xTaskGetTickCount();

    if (press != last_press) 
    {
        last_change = now;
        last_press = press;
    }
    if((now - last_change) * portTICK_PERIOD_MS >= DEBOUNCE_MS) {

        if (press && !stable_state) 
        {
            stable_state = true;
            press_start = now;
        }

        if (!press && stable_state) 
        {
            stable_state = false;
            TickType_t held = now - press_start;
            uint32_t held_ms = pdTICKS_TO_MS(now - press_start);

            if (held_ms >= LONG_POWER_MS)
            {
                waiting_double_press = false;
                return BUTTON_POWER;
            }
            else if (held_ms >= LONG_RESET_MS)
            {
                waiting_double_press = false;
                return BUTTON_RESET;
            }
            else if (held_ms >= LONG_PRESS_MS) 
            {
                waiting_double_press = false;
                return BUTTON_LONG;
            } 
            else
            {
                if (waiting_double_press)
                {
                    waiting_double_press = false;
                    return BUTTON_DOUBLE;
                }
                else
                {
                    waiting_double_press = true;
                    first_press_time = now;
                }
            }
        }
    }
    if (waiting_double_press &&
        pdTICKS_TO_MS(now - first_press_time) > DOUBLE_PRESS_MS)
    {
        waiting_double_press = false;
        return BUTTON_SHORT;
    }
    return BUTTON_NONE;
}
static void button_task(void *arg)
{
    while(1)
    {
        button_event_t state = button_update();

        if (state == BUTTON_SHORT)
        {
            ESP_LOGI(TAG, "FALL ALARM TRIGGERED");
            battery_set(50);
            edge_trigger_event(EVENT_FALLARM);
        }
        else if (state == BUTTON_LONG)
        {
            ESP_LOGI(TAG, "GAS LARM TRIGGERED");
            battery_set(25);
            edge_trigger_event(EVENT_GASLARM);
        }
        else if (state == BUTTON_RESET)
        {
            ESP_LOGW(TAG, "DEVICE RESET TRIGGERED");
            esp_restart();
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void button_init()
{
    gpio_config_t btn_cfg = {
        .pin_bit_mask = 1ULL << BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&btn_cfg);
    
    xTaskCreate(button_task,
                "button",
                2048,
                NULL,
                5,
                NULL);
}