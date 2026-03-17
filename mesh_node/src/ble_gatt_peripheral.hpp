#pragma once

#include <Arduino.h>
#include <ArduinoBLE.h>
#include "edge_event.hpp"
#include "auth_node.hpp"


#define EVENT_HEARTBEAT  0x00 // only need QoS 0
#define EVENT_FALLARM   0x01
#define EVENT_GASLARM   0x02


void ble_init(const char* node_name);
void ble_loop(AuthNode &auth);