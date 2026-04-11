#pragma once

#include <stdint.h>

#define AUTH_TAG_LEN 8

typedef struct __attribute__((packed)) {
    uint8_t device_id;
    uint8_t event_type;
    uint8_t event_location;
    uint8_t battery_status;
    uint8_t seq;
    uint8_t auth_tag[AUTH_TAG_LEN];
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