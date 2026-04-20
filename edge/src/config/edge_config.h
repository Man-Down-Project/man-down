#ifndef EDGE_PROTOCOL_H
#define EDGE_PROTOCOL_H

//Should probably named the file EDGE_CONFIG or something,
//Purpose is to have easy access to the structs needed through out the build

#include <stdint.h>
#include "freertos/FreeRTOS.h"

// --------------------------------------------------------------------------
// >                           Configuration                                <
// --------------------------------------------------------------------------

// BUZZER pin setup
#define BUZZER_GPIO GPIO_NUM_5

// BUTTON pin setup
#define BUTTON_GPIO GPIO_NUM_4

// External RGB LED SETUP
#define LED_RED     GPIO_NUM_0
#define LED_GREEN   GPIO_NUM_1
#define LED_BLUE    GPIO_NUM_2

// Sparkfun BMA400 pin setup
#define I2C_MASTER_SCL_IO   GPIO_NUM_12
#define I2C_MASTER_SDA_IO   GPIO_NUM_22
#define BMA400_INT_PIN      GPIO_NUM_25

// BLE SETUP
#define ROAM_THRESHOLD 16
#define RSSI_SMOOTH_FACTOR 3
#define PAIRING_TIMEOUT 8000
#define ACK_TIMEOUT_MS 300
#define MAX_RETRIES 5
#define SCAN_LENGTH 700
#define MAX_CONNECT_FAILS 3
#define NODE_BLACKLIST_TIME pdMS_TO_TICKS(60000) // <-1min blacklist

//TEST PAYLOAD SETUP
#define AUTH_TAG_LEN 8
#define EVENT_HEARTBEAT 0x00
#define EVENT_FALLARM   0x01
#define EVENT_GASLARM   0x02
#define HEART_TIMER 10000
#define KEY_LEN 16
#define DEVICE_ID 1
//hårdkodad event_location atm
#define BATTERY_STATUS  94
//Seq uppdateras automatiskt i heartbeat packets
typedef struct __attribute__((packed)){ // external comunication
    uint8_t device_id[6];
    uint8_t event_type;
    uint8_t event_location;
    uint8_t battery_status;
    uint8_t seq;
    uint8_t auth_tag[AUTH_TAG_LEN];
}edge_event_t;

typedef struct {
    uint8_t seq;
    uint8_t status;
} edge_ack_t;

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
    BLE_STATE_READY,
    BLE_STATE_SUBSCRIBING
} ble_state_t;

typedef enum {
    MODE_PROVISIONING,
    MODE_NORMAL
} device_mode_t;

extern device_mode_t device_mode;

#endif
