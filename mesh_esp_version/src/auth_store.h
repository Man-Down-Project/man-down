#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "auth.h"
#include "node.h"
#include "auth_common.h"

#define MAX_APPROVED_EDGE 10
#define EMPTY_ID 0xFF

typedef struct {
    uint8_t shared_key[KEY_LEN];
    uint32_t key_timestamp;
} auth_global_t;

typedef struct {
    uint8_t device_whitelist[MAX_APPROVED_EDGE];
    auth_global_t auth;
} auth_storage_t;

void update_global_key(auth_storage_t *store,
                       uint8_t *new_key,
                       uint32_t new_ts);
void save_auth_storage(auth_storage_t *data);
bool load_auth_storage(auth_storage_t *out);
bool load_ca_cert(uint8_t **cert_out, size_t *len_out);
bool store_ca_cert(uint8_t *cert, size_t len);
bool load_hmac_key(uint8_t *key);
bool store_hmac_key(uint8_t *key);
void save_auth_storage(auth_storage_t *data);
bool load_auth_storage(auth_storage_t *out);
bool add_device(auth_storage_t * store, uint8_t device_id);