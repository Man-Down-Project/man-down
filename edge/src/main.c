#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"


#include "ble/ble.h"
#include "ble/ble_gatt_client.h"
#include "config/edge_config.h"
#include "peripherals/button.h"
#include "peripherals/buzzer.h"
#include "peripherals/led.h"
#include "peripherals/sensors/accelerometer.h"
#include "system/system_events.h"
#include "system/event_task.h"


static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting application");
    
    event_task_init();
    ble_init();
    accelerometer_init();
    button_init();
    buzzer_init();
    led_init(); 
}

