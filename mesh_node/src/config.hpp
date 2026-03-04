#pragma once

//node config
#define NODE_ID 1

//wifi config
#define WIFI_SSID "wifi name"
#define WIFI_PASS "wifi password"

//MQTT config
#define MQTT_BROKER "ip"
#define MQTT_PORT 8883

//Topics
#define MQTT_TOPIC_EVENT "mesh/edge_event"

//System limits
#define MAX_PAYLOAD 8
#define AUTH_TAG_LEN 8 // unneccesary?
#define KEY_LEN 16
#define EEPROM_START 0
#define MAX_TOPIC_SIZE 64

#define MAX_NEIGHBORS 5
#define MAX_APPROVED_EDGE 10
#define MAX_EDGE_DEVICES 256
#define BLE_ACK_SIZE 2

