#ifndef AUTH_H
#define AUTH_H

#include <stdint.h>
#include <stddef.h>
#include "../config/user_config.h"
typedef struct {
    uint8_t device_id;
    uint8_t seq;
    uint8_t event;
    uint32_t timestamp;
    uint8_t auth_tag[AUTH_TAG_LEN];
} edge_pkt_t;
void generate_auth_tag(uint8_t *data, size_t data_len, uint8_t *auth_tag);
bool auth_key_exists(void);
void auth_store_key(const uint8_t *key, size_t len);
#endif