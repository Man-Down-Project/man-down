#ifndef AUTH_H
#define AUTH_H

#include <stdint.h>
#include <stddef.h>
#include "config/edge_config.h"
typedef struct {
    uint8_t device_id;
    uint8_t seq;
    uint8_t event;
    uint32_t timestamp;
    uint8_t auth_tag[AUTH_TAG_LEN];
} edge_pkt_t;
void generate_auth_tag(uint8_t *data, size_t data_len, uint8_t *auth_tag);

#endif