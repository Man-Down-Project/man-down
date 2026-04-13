#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"

#include "nimble/hci_common.h"

#include "events/node.h"
#include "wifi_ble/ble.h"
#include "wifi_ble/wifi.h"
#include "events/event_task.h"
#include "peripherals/onboard_led.h"
// #include "mqtt_client.h"
// #include "boot.h"

static const char *TAG = "[MAIN]";


void app_main() 
{
    ESP_LOGI(TAG, "Starting mesh node");
    
    nvs_init();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_sta();
    ble_init();
    node_init();
    event_task_init();
    onboard_led_init();
   
}