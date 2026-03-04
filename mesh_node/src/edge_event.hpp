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