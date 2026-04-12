#ifndef AUTH_H
#define AUTH_H

#include <stdint.h>
#include <stddef.h>
#include "../config/user_config.h"
#include "auth_store.h"

#define KEY_LEN 16
typedef struct {
    uint8_t device_id;
    uint8_t seq;
    uint8_t event;
    uint32_t timestamp;
    uint8_t auth_tag[AUTH_TAG_LEN];
} edge_pkt_t;
void auth_store_key(const uint8_t *key, size_t len);
bool verify_edge_message(uint8_t *data, size_t data_len);
#endif