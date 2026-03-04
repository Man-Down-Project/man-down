#pragma once

#include <Arduino.h>
#include <ArduinoBLE.h>
#include "edge_event.hpp"
#include "auth_node.hpp"


#define EVENT_HEARTBEAT  0x00 // only need QoS 0


void ble_init(const char* node_name);
void ble_poll(AuthNode &auth);