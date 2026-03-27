#pragma once
#include "personal_setup.hpp"
#include "../certs/ca_cert.hpp"

//System mode switch
#define USE_EEPROM  1 


//node config
#define NODE_ID 1
#define PARENT_IP "192.168.0.2"
#define MAX_BACKUP_PARENTS 3
#define NODE_DEPTH 1

// MQTT client ID and Topics
#define MQTT_TOPIC_EVENT "mesh/node/%d/edge" //For TLS

//System limits
#define MAX_PAYLOAD 21
#define AUTH_TAG_LEN 8 
#define KEY_LEN 16
#define EEPROM_START 0
#define MAX_TOPIC_SIZE 64

#define MAX_NEIGHBORS 5
#define MAX_APPROVED_EDGE 10
#define MAX_EDGE_DEVICES 256
#define BLE_ACK_SIZE 2

#define WIFI_RETRY_INTERVAL 5000
#define MQTT_RETRY_INTERVAL 5000

static const char* BACKUP_PARENTS[MAX_BACKUP_PARENTS] = {
    "192.168.0.106", //fake
    "192.168.0.107", //fake
    "192.168.0.108" //fake
};