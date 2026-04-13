#pragma once

#include <stdbool.h>
#include "mqtt_client.h"

extern bool wifi_connected_globally;
extern esp_mqtt_client_handle_t mqtt_client;

void wifi_init_sta(void);