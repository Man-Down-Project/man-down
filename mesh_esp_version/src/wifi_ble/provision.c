#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "storage/auth_store.h"
#include "security/auth.h"

static const char *TAG = "[PROVISIONING]";

// 🔥 Convert hex string → bytes
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

// 🔥 MAIN FUNCTION (matches Arduino)
void handle_provision(uint8_t *data, int len)
{
    
    uint8_t existing_key[KEY_LEN];
    if (load_hmac_key(existing_key)) {
        ESP_LOGW(TAG, "Key already provisioned, skipping");
        return;
    }
    
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

    uint32_t timestamp = strtoul(tsStr, NULL, 10);

    uint8_t key[16];

    if (!hexStringToByte(keyStr, key, 16)) {
        ESP_LOGE(TAG, "Invalid key format");
        return;
    }

    // 🔥 store key in NVS
    if (!store_hmac_key(key)) {
        ESP_LOGE(TAG, "Failed to store key");
        return;
    }

    ESP_LOGI(TAG, "HMAC key stored successfully");

    // 🔥 reboot so new key is used
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}