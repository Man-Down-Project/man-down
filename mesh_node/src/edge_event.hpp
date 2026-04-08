#pragma once
#include <stdint.h>
#include "config.hpp"

typedef struct {
    uint8_t device_id;
    uint8_t event_type;
    uint8_t location;
    uint8_t battery;
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

struct runtime_compare {
    uint8_t device_id[MAX_APPROVED_EDGE];
    uint8_t last_seq[MAX_APPROVED_EDGE];
};