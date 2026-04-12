# List your source files explicitly to avoid build-order headaches
set(app_sources 
    "main.c" 
    "ble.c" 
    "node.c" 
    "wifi.c" 
    "mqtt.c"
    "auth.c"
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
)