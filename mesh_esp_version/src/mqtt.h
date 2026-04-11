#pragma once

#include "mqtt_client.h"
#include "stdbool.h"

extern esp_mqtt_client_handle_t mqtt_client;
extern bool mqtt_connected;

void mqtt_app_start(const char* uri, const char* user, const char* pass);