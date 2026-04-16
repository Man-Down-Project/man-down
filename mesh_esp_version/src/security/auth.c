#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#include "mbedtls/md.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "events/node.h"
#include "auth.h"
#include "storage/auth_store.h"
#include "freertos/FreeRTOS.h"

#include "wifi_ble/provision.h"

static const char *TAG = "[AUTH]";

uint8_t shared_key[KEY_LEN] = {
    0x64, 0xF9, 0x0E, 0xE7, 0x0E, 0xB4, 0x0E, 0x78,
    0x4F, 0xFE, 0xF2, 0xEF, 0x96, 0x01, 0xB1, 0x4F
};

bool auth_load_key(uint8_t *out_key, size_t *out_len)
{
    if (!out_key || !out_len) {
        ESP_LOGE(TAG, "Invalid arguments to auth_load_key");
        return false;
    }

    nvs_handle_t handle;
    *out_len = KEY_LEN;

    esp_err_t err = nvs_open("auth", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_get_blob(handle, "hmac_secret", out_key, out_len);
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "HMAC key not found in NVS: %s", esp_err_to_name(err));
        secure_memzero(out_key, KEY_LEN);
        return false;
    }

    if (*out_len != KEY_LEN) {
        ESP_LOGE(TAG, "Invalid key length: %d (expected %d)", (int)*out_len, KEY_LEN);
        secure_memzero(out_key, KEY_LEN);
        return false;
    }

    return true;
}

void auth_store_key(const uint8_t *key, size_t len) {
    nvs_handle_t handle;
    
    esp_err_t err = nvs_open("auth", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_blob(handle, "hmac_secret", key, len);
    if (err == ESP_OK) {
        
        err = nvs_commit(handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Key successfully OVERWRITTEN and committed to NVS");
        } else {
            ESP_LOGE(TAG, "NVS Commit failed: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "Failed to set NVS blob: %s", esp_err_to_name(err));
    }

    nvs_close(handle);
    vTaskDelay(pdMS_TO_TICKS(100));
}

bool verify_edge_message(uint8_t *data, size_t data_len) {
    
    if (!data || data_len != sizeof(edge_event_t)) {
        ESP_LOGE(TAG, "Invalid input length: %d (expected %zu)",
                 (int)data_len, sizeof(edge_event_t));
        return false;
    }

    uint8_t received_tag[AUTH_TAG_LEN];
    uint8_t calculated_hash[32];
    uint8_t active_key[KEY_LEN];
    size_t key_len = KEY_LEN; 

    if (!auth_load_key(active_key, &key_len))
    {
        ESP_LOGE(TAG, "No key provisioned! rejecting message.");
        secure_memzero(active_key, sizeof(active_key));
        return false;
    }

    uint8_t buffer[sizeof(edge_event_t)];
    memcpy(buffer, data, data_len);

    size_t tag_offset = data_len - AUTH_TAG_LEN;
    memcpy(received_tag, buffer + tag_offset, AUTH_TAG_LEN);

    printf("[DEBUG] RX: ");
    for (int i = 0; i < data_len; i++) printf("%02X", buffer[i]);
    printf("\n");

    printf("[DEBUG] KEY: ");
    for (int i = 0; i < KEY_LEN; i++) printf("%02X", active_key[i]);
    printf("\n");

    memset(buffer + tag_offset, 0, AUTH_TAG_LEN);

    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, md, 1);

    mbedtls_md_hmac_starts(&ctx, active_key, KEY_LEN);
    mbedtls_md_hmac_update(&ctx, buffer, offsetof(edge_event_t, auth_tag));
    mbedtls_md_hmac_finish(&ctx, calculated_hash);
    mbedtls_md_free(&ctx);

    bool result;

    if (memcmp(calculated_hash, received_tag, AUTH_TAG_LEN) == 0) {
        ESP_LOGI(TAG, "HMAC Verified");
        result = true;
    } else {
        ESP_LOGE(TAG, "HMAC Mismatch");
        result = false;
    }

    secure_memzero(active_key, sizeof(active_key));
    secure_memzero(calculated_hash, sizeof(calculated_hash));
    secure_memzero(buffer, sizeof(buffer));
    secure_memzero(received_tag, sizeof(received_tag));

    return result;
}

void generate_auth_tag(uint8_t *data, size_t data_len, uint8_t *auth_tag)
{
    uint8_t calculated_hash[32];
    uint8_t active_key[KEY_LEN];
    size_t key_len = KEY_LEN;

    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, md, 1);

    if (!auth_load_key(active_key, &key_len)) {
        memcpy(active_key, shared_key, KEY_LEN);
    }

    mbedtls_md_hmac_starts(&ctx, active_key, KEY_LEN);
    mbedtls_md_hmac_update(&ctx, data, data_len);
    mbedtls_md_hmac_finish(&ctx, calculated_hash);
    memcpy(auth_tag, calculated_hash, AUTH_TAG_LEN);
    mbedtls_md_free(&ctx);

    secure_memzero(active_key, sizeof(active_key));
    secure_memzero(calculated_hash, sizeof(calculated_hash));
}