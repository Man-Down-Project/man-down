#pragma once
#include <stdint.h>

typedef struct {
    uint8_t device_id;
    uint8_t event_type;
    uint8_t location;
    uint8_t battery;
    uint8_t seq;
} edge_event_t;