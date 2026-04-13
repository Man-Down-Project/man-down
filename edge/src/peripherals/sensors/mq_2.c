#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_log.h"
#include "esp_task_wdt.h"

#include "system/event_task.h"

#define MQ2_GPIO GPIO_NUM_11

#define GAS_THRESHOLD 2000
#define GAS_HYSTERESIS 200
static bool gas_active = false;
static QueueHandle_t gas_event_queue = NULL;
static const char *TAG = "[MQ2_GAS]";

typedef struct {
    int level;
} gas_event_t;

static void IRAM_ATTR mq2_isr_handler(void *arg)
{
    int level = gpio_get_level(MQ2_GPIO);

    gas_event_t event = {
        .level = level
    };

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(gas_event_queue, &event, &xHigherPriorityTaskWoken);
    if(xHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }
}

static void gas_task(void *arg)
{
    gas_event_t event;
    int last_state = -1;

    static uint32_t last_check_time = 0;
    static uint32_t last_gas_event = 0;

    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    while (1)
    {
        if (xQueueReceive(gas_event_queue, &event, pdMS_TO_TICKS(200)))
        {
            uint32_t now = xTaskGetTickCount();

            // ⛔ Skip too frequent processing (debounce)
            if (now - last_check_time < pdMS_TO_TICKS(200)) {
                continue;
            }
            last_check_time = now;

            int stable_level = gpio_get_level(MQ2_GPIO);

            ESP_LOGI(TAG, "Raw GPIO level: %d | Stable: %d", event.level, stable_level);

            // 🔹 Edge detection
            if (stable_level != last_state)
            {
                if (stable_level == 1)
                {
                    if (now - last_gas_event > pdMS_TO_TICKS(10000))
                    {
                        ESP_LOGI(TAG, "Gas detected!");
                        system_event_post(EVENT_GAS_ALARM, 0);
                        last_gas_event = now;
                    }
                }
                else
                {
                    ESP_LOGI(TAG, "Gas cleared");
                }

                last_state = stable_level;
            }
        }
        esp_task_wdt_reset();
    }
}

void mq2_init(void)
{
    gas_event_queue = xQueueCreate(10, sizeof(gas_event_t));
    
    gpio_config_t mq2_conf = {
        .pin_bit_mask = (1ULL << MQ2_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE
    };
    gpio_config(&mq2_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(MQ2_GPIO, mq2_isr_handler, NULL);

    xTaskCreate(gas_task,
                "gas_task",
                 2048,
                 NULL,
                 7,
                 NULL);
    
    ESP_LOGI(TAG, "MQ-2 initialized on GPIO %d", MQ2_GPIO);
}