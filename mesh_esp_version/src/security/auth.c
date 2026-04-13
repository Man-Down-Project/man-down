#include <stdint.h>
#include <string.h>
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

static const char *TAG = "[AUTH]";

uint8_t shared_key[KEY_LEN] = {
    0x64, 0xF9, 0x0E, 0xE7, 0x0E, 0xB4, 0x0E, 0x78,
    0x4F, 0xFE, 0xF2, 0xEF, 0x96, 0x01, 0xB1, 0x4F
};

bool auth_load_key(uint8_t *out_key, size_t *out_len) {
    nvs_handle_t handle;
    if (nvs_open("auth", NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    esp_err_t err = nvs_get_blob(handle, "hmac_secret", out_key, out_len);
    nvs_close(handle);
    return (err == ESP_OK && *out_len == KEY_LEN);
}

void auth_store_key(const uint8_t *key, size_t len) {
    nvs_handle_t handle;
    if (nvs_open("auth", NVS_READWRITE, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS");
        return;
    }
    nvs_set_blob(handle, "hmac_secret", key, len);
    nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGI(TAG, "Key stored in NVS");
    vTaskDelay(pdMS_TO_TICKS(100));
}

/*
 * ✅ Correct function
 * - data: pointer to EXACT struct bytes
 * - data_len: MUST be sizeof(edge_event_t)
 */
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

    // Load key or fallback
    bool has_provisioned_key = load_hmac_key(active_key);

    if (has_provisioned_key) 
    {
        ESP_LOGI(TAG, "Using provisioned key");
    } else {
        ESP_LOGW(TAG, "No provisioned key, using fallback");
        memcpy(active_key, shared_key, KEY_LEN);
    }

    // Copy into fixed-size buffer (SAFE)
    uint8_t buffer[sizeof(edge_event_t)];
    memcpy(buffer, data, data_len);

    // auth_tag is last field in struct
    size_t tag_offset = data_len - AUTH_TAG_LEN;

    // Extract received tag
    memcpy(received_tag, buffer + tag_offset, AUTH_TAG_LEN);

    // Debug prints
    printf("[DEBUG] RX: ");
    for (int i = 0; i < data_len; i++) printf("%02X", buffer[i]);
    printf("\n");

    printf("[DEBUG] TAG: ");
    for (int i = 0; i < AUTH_TAG_LEN; i++) printf("%02X", received_tag[i]);
    printf("\n");

    printf("[DEBUG] KEY: ");
    for (int i = 0; i < KEY_LEN; i++) printf("%02X", active_key[i]);
    printf("\n");

    // Zero tag before hashing
    memset(buffer + tag_offset, 0, AUTH_TAG_LEN);

    // HMAC
    const mbedtls_md_info_t *md =
    mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, md, 1);

    mbedtls_md_hmac_starts(&ctx, active_key, KEY_LEN);
    mbedtls_md_hmac_update(&ctx, buffer, offsetof(edge_event_t, auth_tag));
    mbedtls_md_hmac_finish(&ctx, calculated_hash);

    mbedtls_md_free(&ctx);

    // Compare (only first AUTH_TAG_LEN bytes)
    if (memcmp(calculated_hash, received_tag, AUTH_TAG_LEN) == 0) {
        ESP_LOGI(TAG, "HMAC Verified");
        return true;
    } else {
        ESP_LOGE(TAG, "HMAC Mismatch");

        printf("[DEBUG] CALC: ");
        for (int i = 0; i < AUTH_TAG_LEN; i++) printf("%02X", calculated_hash[i]);
        printf("\n");

        return false;
    }
}

