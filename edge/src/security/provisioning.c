#include "nvs_flash.h"

#include "provisioning.h"
#include "config/edge_config.h"
#include "security/auth.h"
#include "ble/ble_core.h"
#include "ble/ble_gap.h"
#include "ble/ble_gatt_client.h"
#include "peripherals/led.h"
#include "esp_system.h"

#define HMAC_KEY_SIZE 16
static const char *TAG = "[PROVISION]";
static bool active = false;


void provisioning_init(void)
{
    bool has_key = auth_key_exists();
    ESP_LOGI(TAG, "Key exists: %s", has_key ? "YES" : "NO");
    active = !has_key;
    ESP_LOGI(TAG, "Provisioning mode: %s", active ? "ON" : "OFF");

    if (active)
    {
        led_set(RGB_MAGENTA, LED_MODE_BLINK, LED_PRIO_MEDIUM);
    }
}

bool provisioning_is_active(void)
{
    return active;
}

void provisioning_on_scan_match(const ble_addr_t *addr)
{
    if (!active) return;
    ESP_LOGI(TAG, "Provisioning device found -> connecting");
    ble_gap_connect_to(addr);
}

void provisioning_on_connected(uint16_t conn_handle)
{
    if (!active) return;
    ESP_LOGI(TAG, "Connected for provisioning");
    led_set(RGB_MAGENTA, LED_MODE_SOLID, LED_PRIO_MEDIUM);
}

void provisioning_handle_rx(const uint8_t *data, size_t len)
{
    if (!active) return;

    ESP_LOGI(TAG, "Received provisioning data (%d bytes)", len);
    if (len != HMAC_KEY_SIZE) {
        ESP_LOGW(TAG, "Invalid key length (%d)", len);
        return;
    }
    auth_store_key(data, len);

    ESP_LOGI(TAG, "Provisioning complete!");
    ESP_LOGI(TAG, "Restarting device!");
    
    active = false;
    ble_clear_bonds();
    ble_disconnect();
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    
}

