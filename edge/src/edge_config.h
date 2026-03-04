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
    uint8_t auth_key;
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
    BLE_STATE_DISCOVERING,
    BLE_STATE_READY
} ble_state_t;

#define EVENT_HEARTBEAT 0x00
#define EVENT_FALLARM   0x01
#define EVENT_GASLARM   0x02

#define HEART_TIMER 10000

//TEST SETUP

#define DEVICE_ID 1
#define EVENT_HEARTBEAT 0x00
//hårdkodad event_location atm
#define BATTERY_STATUS  94
//Seq uppdateras automatiskt i heartbeat packets
#define AUTH_KEY 0

#endif