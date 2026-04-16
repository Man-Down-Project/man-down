#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "storage/auth_store.h"
#include "security/auth.h"

static const char *TAG = "[PROVISIONING]";

void secure_memzero(void *v, size_t n);

bool hexStringToByte(const char* str, uint8_t* out, size_t outLen)
{
    size_t len = strlen(str);
    if (len != outLen * 2)
        return false;

    for (int i = 0; i < outLen; i++) {
        char byteStr[3] = { str[i*2], str[i*2+1], 0 };
        out[i] = (uint8_t)strtoul(byteStr, NULL, 16);
    }
    return true;
}

void handle_provision(uint8_t *data, int len)
{
    if (len != 40) {
        ESP_LOGE(TAG, "Invalid HMAC payload length: %d", len);
        return;
    }

    char buffer[41];
    memcpy(buffer, data, 40);
    buffer[40] = '\0';

    char keyStr[33];
    char tsStr[9];

    memcpy(keyStr, buffer, 32);
    keyStr[32] = '\0';

    memcpy(tsStr, buffer + 32, 8);
    tsStr[8] = '\0';

    ESP_LOGI(TAG, "Key string: %s", keyStr);
    ESP_LOGI(TAG, "Timestamp string: %s", tsStr);

    uint8_t incoming_key[16];
    if (!hexStringToByte(keyStr, incoming_key, 16)) {
        ESP_LOGE(TAG, "Invalid key format");
        return;
    }

    uint8_t existing_key[16];
    size_t existing_len = 16;
    
    bool has_key = auth_load_key(existing_key, &existing_len);

    if (has_key && memcmp(incoming_key, existing_key, 16) == 0) 
    {
        ESP_LOGI(TAG, "Provisioning match: Key already up to date");

        secure_memzero(incoming_key, sizeof(incoming_key));
        secure_memzero(existing_key, sizeof(existing_key));
        secure_memzero(buffer, sizeof(buffer));
        secure_memzero(keyStr, sizeof(keyStr));
        secure_memzero(tsStr, sizeof(tsStr));

        return;
    }

    ESP_LOGI(TAG, "Provisioning mismatch: Writing new key to NVS...");
   
    auth_store_key(incoming_key, 16);

    secure_memzero(existing_key, sizeof(existing_key));
    
    nvs_handle_t nvs;
    if (nvs_open("config", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_u8(nvs, "prov", 1);
        nvs_commit(nvs);
        nvs_close(nvs);
    }

    secure_memzero(incoming_key, sizeof(incoming_key));
    secure_memzero(buffer, sizeof(buffer));
    secure_memzero(keyStr, sizeof(keyStr));
    secure_memzero(tsStr, sizeof(tsStr));

    ESP_LOGI(TAG, "HMAC key stored successfully. Rebooting...");

    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}

void secure_memzero(void *v, size_t n)
{
    volatile uint8_t *p = v;
    while (n--) *p++ = 0;
}

bool is_provisioned() 
{
    nvs_handle_t nvs;

    if (nvs_open("config", NVS_READONLY, &nvs) != ESP_OK)
        return false;

    uint8_t flag = 0;
    esp_err_t err = nvs_get_u8(nvs, "prov", &flag);

    nvs_close(nvs);

    return (err == ESP_OK && flag == 1);
}