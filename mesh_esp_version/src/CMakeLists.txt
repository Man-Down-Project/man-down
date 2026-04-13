# List your source files explicitly to avoid build-order headaches
set(app_sources 
    "main.c" 
    "wifi_ble/ble.c" 
    "events/node.c" 
    "wifi_ble/wifi.c" 
    "wifi_ble/mqtt.c"
    "security/auth.c"
    "storage/auth_store.c"
    "wifi_ble/provision.c"
    "peripherals/onboard_led.c"
    "events/event_task.c"
)

idf_component_register(SRCS ${app_sources}
                       INCLUDE_DIRS "."
                       REQUIRES
                           bt
                           nvs_flash
                           mqtt
                           esp_event
                           esp_wifi
                           esp_netif
                           led_strip
)