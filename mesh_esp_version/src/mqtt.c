#include "esp_log.h"

#include "mqtt.h"
#include "../config/user_config.h"

extern const uint8_t broker_ca_cert_pem_start[] asm("_binary_broker_ca_cert_pem_start");
extern const uint8_t broker_ca_cert_pem_end[]   asm("_binary_broker_ca_cert_pem_end");
static const char *TAG = "[MQTT]";

esp_mqtt_client_handle_t mqtt_client = NULL;
bool mqtt_connected = false;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) 
    {
        case MQTT_EVENT_CONNECTED:
        {
            ESP_LOGI(TAG, "Connected to Broker");
            mqtt_connected = true;
            break;
        }
        case MQTT_EVENT_DISCONNECTED:
        {
            ESP_LOGW(TAG, "Disconnected from Broker");
            mqtt_connected = false;
            break;
        }
        case MQTT_EVENT_ERROR:
        {
            ESP_LOGE(TAG, "MQTT Error Type: %d", event->error_handle->error_type);
            break;
        }
        default:
        {
            break;
        }
    }
}

void mqtt_app_start(const char* uri, const char* user, const char* pass)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = uri,
        .broker.verification.certificate = ca_cert,
        .credentials.username = user,
        .credentials.authentication.password = pass,
        .broker.verification.skip_cert_common_name_check = true,
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}

