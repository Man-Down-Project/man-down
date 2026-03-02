#pragma once

#include <Arduino.h>
#include <ArduinoBLE.h>

#define MAX_PAYLOAD 8
#define EVENT_HEARTBEAT  0x00 // only need QoS 0

typedef struct {
    uint8_t device_id;
    uint8_t event_type;
    uint8_t location;
    uint8_t battery;
    uint8_t seq;
} edge_event_t;

void ble_init(const char* node_name);
void ble_poll();