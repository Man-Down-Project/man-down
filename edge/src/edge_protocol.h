#ifndef EDGE_PROTOCOL_H
#define EDGE_PROTOCOL_H

//Should probably named the file EDGE_CONFIG or something,
//Purpose is to have easy access to the structs needed through out the build

#include <stdint.h>
typedef struct { // external comunication
    uint8_t device_id;
    uint8_t event_type;
    uint8_t event_location;
    uint8_t battery_status;
    uint8_t seq;
}edge_event_t;

typedef struct {
    uint8_t accelerometer;
    uint8_t last_location;
    uint8_t current_node;
}internal_data;

typedef enum {
    BLE_STATE_IDLE,
    BLE_STATE_SCANNING,
    BLE_STATE_CONNECTING,
    BLE_STATE_CONNECTED,
    BLE_STATE_DISCOVERING
} ble_state_t;


#endif