#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "mbedtls/md.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "config/edge_config.h"

static bool key_present = false;
static uint8_t stored_key[16];
static const char *TAG = "[AUTH]";
bool auth_load_key(uint8_t *out_key, size_t *out_len);

uint8_t shared_key[KEY_LEN] = {
    0x9A,0x4F,0x21,0xC7,
    0x55,0x13,0xE8,0x02,
    0x6D,0xB9,0x33,0xA1,
    0x7C,0x4D,0x90,0xEE
};

void generate_auth_tag(uint8_t *data, size_t data_len, uint8_t *auth_tag)
{
    uint8_t full_hash[32];   // SHA256 output
    uint8_t secret_buf[KEY_LEN];
    size_t key_len = KEY_LEN;

    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, md, 1);

    if (!auth_load_key(secret_buf, &key_len)) {
        ESP_LOGW(TAG, "Using fallback shared_key");
        memcpy(secret_buf, shared_key, KEY_LEN);
    }
    ESP_LOGI(TAG, "Using key:");
    for (int i = 0; i < KEY_LEN; i++) {
        printf("%02X ", secret_buf[i]);
    }
    mbedtls_md_hmac_starts(&ctx, secret_buf, KEY_LEN);

    // Hash the packet data
    mbedtls_md_hmac_update(&ctx, data, data_len);

    // Finish HMAC
    mbedtls_md_hmac_finish(&ctx, full_hash);

    // Store truncated tag
    memcpy(auth_tag, full_hash, AUTH_TAG_LEN);

    mbedtls_md_free(&ctx);
}

bool auth_key_exists(void)
{
    nvs_handle_t handle;
    size_t len = 0;
    
    if (nvs_open("auth", NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    esp_err_t err = nvs_get_blob(handle, "hmac_secret", NULL, &len);
    nvs_close(handle);

    return (err == ESP_OK && len == KEY_LEN);
}
void auth_store_key(const uint8_t *key, size_t len)
{
    nvs_handle_t handle;
    if (nvs_open("auth", NVS_READWRITE, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS");
        return;
    }
    nvs_set_blob(handle, "hmac_secret", key, len);
    nvs_commit(handle);
    nvs_close(handle);

    ESP_LOGI(TAG, "Key stored in NVS");

    ESP_LOGI(TAG, "---- KEY DEBUG ----");
    ESP_LOGI(TAG, "Provisioned key:");
    for (int i = 0; i < len; i++) {
        printf("%02X ", key[i]);
    }
    printf("\n");
    ESP_LOGI(TAG, "Shared key :");
     for (int i = 0; i < len; i++) {
        printf("%02X ", shared_key[i]);
    }
    printf("\n");
    ESP_LOGI(TAG, "-------------------");
}

bool auth_load_key(uint8_t *out_key, size_t *out_len)
{
    nvs_handle_t handle;

    if (nvs_open("auth", NVS_READONLY, &handle) != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed, using fallback key");
        return false;
    }
    size_t len = KEY_LEN;

    esp_err_t err = nvs_get_blob(handle, "hmac_secret", out_key, &len);
    nvs_close(handle);

    if (err != ESP_OK || len != KEY_LEN) {
        ESP_LOGW(TAG, "No valid key in NVS...");
        return false;
    }

    ESP_LOGI(TAG, "Loaded key from NVS");
    *out_len = len;
    return true;
}