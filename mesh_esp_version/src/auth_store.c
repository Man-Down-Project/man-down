#include <stdbool.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#include "auth_store.h"

#define NVS_NAMESPACE "auth"
static const char *TAG = "[STORAGE]";

bool load_auth_storage(auth_storage_t *out)
{
    nvs_handle_t handle;
    size_t len = sizeof(auth_storage_t);

    if (nvs_open("auth", NVS_READONLY, &handle) != ESP_OK)
    {
        return false;
    }

    esp_err_t err = nvs_get_blob(handle, "store", out, &len);
    nvs_close(handle);
    return (err == ESP_OK);
}

void save_auth_storage(auth_storage_t *data)
{
    nvs_handle_t handle;
    if (nvs_open("auth", NVS_READWRITE, &handle) != ESP_OK)
    {
        return;
    }

    nvs_set_blob(handle, "store", data, sizeof(auth_storage_t));
    nvs_commit(handle);
    nvs_close(handle);
}

void update_global_key(auth_storage_t *store,
                       uint8_t *new_key,
                       uint32_t new_ts)
{
    if (!new_key || new_ts == 0)
    {
        return;
    }
    memcpy(store->auth.shared_key, new_key, KEY_LEN);
    store->auth.key_timestamp = new_ts;
    save_auth_storage(store);
}

bool store_hmac_key(uint8_t *key)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK)
    {
        return false;
    }
    nvs_set_blob(h, "hmac", key, KEY_LEN);
    nvs_commit(h);
    nvs_close(h);
    return true;
}

bool load_hmac_key(uint8_t *key)
{
    nvs_handle_t h;
    size_t len = KEY_LEN;

    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK)
    {
        return false;
    }
    esp_err_t err = nvs_get_blob(h, "hmac", key, &len);
    nvs_close(h);

    return (err == ESP_OK && len == KEY_LEN);
}

bool store_ca_cert(uint8_t *cert, size_t len)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK)
    {
        return false;
    }
    nvs_set_blob(h, "ca_cert", cert, len);
    nvs_commit(h);
    nvs_close(h);
    return true;
}

bool load_ca_cert(uint8_t **cert_out, size_t *len_out)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK)
    {
        return false;
    }

    size_t len;
    if (nvs_get_blob(h, "ca_cert", NULL, &len) != ESP_OK)
    {
        nvs_close(h);
        return false;
    }

    uint8_t *buf = malloc(len);
    if (!buf)
    {
        nvs_close(h);
        return false;
    }

    if (nvs_get_blob(h, "ca_cert", buf, &len) != ESP_OK)
    {
        free(buf);
        nvs_close(h);
        return false;
    }
    nvs_close(h);

    *cert_out = buf;
    *len_out = len;
    return true;
}

bool add_device(auth_storage_t * store, uint8_t device_id)
{
    for (int i = 0; i < MAX_APPROVED_EDGE; i++) {
        if (store->device_whitelist[i] == EMPTY_ID) 
        {
            store->device_whitelist[i] = device_id;
            return true;
        }
    }
    return false;
}