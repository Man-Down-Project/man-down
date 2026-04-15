#pragma once
#include <stdint.h>
#include "config.hpp"

typedef struct{
    uint8_t mac[MAC_LEN];
}device_id_t;

typedef struct {
    device_id_t device_id;
    uint8_t event_type;
    uint8_t location;
    uint8_t battery;
    uint8_t seq;
    uint8_t auth_tag[AUTH_TAG_LEN];
} edge_event_t;

typedef struct __attribute__((packed)) {
    device_id_t device_id;
    uint8_t event_type;
    uint8_t location;
    uint8_t battery;
    uint8_t seq;
    uint16_t timestamp;
    uint8_t hmac[16];
}edge_event_out;

struct runtime_compare {
    uint8_t device_mac[MAX_APPROVED_EDGE][MAC_LEN];
    uint8_t last_seq[MAX_APPROVED_EDGE];
};