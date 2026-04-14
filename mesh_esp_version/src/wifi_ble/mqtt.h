#pragma once

#include "mqtt_client.h"
#include "stdbool.h"

extern esp_mqtt_client_handle_t mqtt_client;
extern bool mqtt_connected;

// extern const char ca_cert[];
// extern const char client_key[];
// extern const char client_cert[];

void mqtt_app_start(const char* uri, const char* user, const char* pass);