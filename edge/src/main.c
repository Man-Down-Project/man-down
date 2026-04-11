#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_task_wdt.h"


#include "ble/ble.h"
#include "ble/ble_gatt_client.h"
#include "config/edge_config.h"
#include "peripherals/button.h"
#include "peripherals/buzzer.h"
#include "peripherals/led.h"
#include "peripherals/onboard_led.h"
#include "peripherals/sensors/accelerometer.h"
#include "system/system_events.h"
#include "system/event_task.h"
#include "security/provisioning.h"
#include "peripherals/sensors/mq_2.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting application");
    
    nvs_flash_init();
    provisioning_init();
    event_task_init();
    ble_init();
    accelerometer_init();
    button_init();
    buzzer_init();
    led_init();
    onboard_led_init(); 
    mq2_init();
}

