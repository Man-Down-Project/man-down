#pragma once

#include <stdint.h>
#include <stddef.h>

#define AUTH_TAG 8
#define AUTH_TAG_LEN 8
#define CMD_AUTH_FAIL 0xFD 
#define CMD_DISCONNECT 0xFE

typedef struct __attribute__((packed)) {
    uint8_t device_id;
    uint8_t event_type;
    uint8_t event_location;
    uint8_t battery_status;
    uint8_t seq;
    uint8_t auth_tag[AUTH_TAG];
} edge_event_t;

typedef struct __attribute__((packed)) {
    uint8_t device_id;
    uint8_t event_type;
    uint8_t location;
    uint8_t battery;
    uint8_t seq;
    uint16_t timestamp;
}edge_event_out;

typedef struct {
    uint8_t seq;
    uint8_t status;
} node_ack_t;

void node_init(void);

uint16_t GetTimeStamp(void);
void mqtt_publisher_edge_event(edge_event_out *msg);
void send_data_to_edge(uint8_t device_id, uint8_t *data, size_t len);
void disconnect_edge_device(uint8_t device_id);