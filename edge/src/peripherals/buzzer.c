#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_task_wdt.h"

#include "config/edge_config.h"
#include "buzzer.h"



static const char *TAG = "BUZZER";

static buzzer_pattern_t current_pattern = BUZZER_NONE;

void buzzer_beep_loop(uint16_t delay_ms, uint8_t count);

static void buzzer_on()
{
    gpio_set_level(BUZZER_GPIO, 1);
}

static void buzzer_off()
{
    gpio_set_level(BUZZER_GPIO, 0);
}

static void buzzer_task(void *arg)
{
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    while(1)
    {
        switch (current_pattern)
        {
            case BUZZER_FALL:
            {
                buzzer_beep_loop(100, 2);
                vTaskDelay(pdMS_TO_TICKS(200));
                buzzer_beep_loop(300, 3);
                vTaskDelay(pdMS_TO_TICKS(200));
                buzzer_beep_loop(100, 2);
                break;
            }
            case BUZZER_GAS:
            {
                buzzer_on();
                vTaskDelay(pdMS_TO_TICKS(1000));
                buzzer_off();
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
            }
            case BUZZER_LOW_BATTERY:
            {
                for (int i = 0; i < 2; i++)
                {
                    buzzer_on();
                    vTaskDelay(pdMS_TO_TICKS(100));
                    buzzer_off();
                    vTaskDelay(pdMS_TO_TICKS(400));
                }
                current_pattern = BUZZER_NONE;
                break;
            }
            default: 
                vTaskDelay(pdMS_TO_TICKS(50));

        }
        esp_task_wdt_reset();
    }
}

void buzzer_beep_loop(uint16_t delay_ms, uint8_t count)
{
    for (int i = 0; i < count; i++)
    {
        buzzer_on();
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        buzzer_off();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void buzzer_play(buzzer_pattern_t pattern)
{
    ESP_LOGI(TAG, "pattern=%d", pattern);
    current_pattern = pattern;
}

void buzzer_init()
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << BUZZER_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&cfg);

    xTaskCreate(buzzer_task,
                "buzzer",
                2048,
                NULL,
                7,
                NULL);
}

void buzzer_stop()
{
    current_pattern = BUZZER_NONE;
    buzzer_off();
}