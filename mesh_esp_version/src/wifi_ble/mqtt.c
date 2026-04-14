#include "esp_log.h"
#include "mqtt_client.h"

#include "mqtt.h"
#include "config/user_config.h"
#include "storage/auth_store.h"
#include "provision.h"
#include "string.h"
#include "security/auth.h"
#include "esp_wifi.h"
#include "esp_mac.h"

// extern const uint8_t broker_ca_cert_pem_start[] asm("_binary_broker_ca_cert_pem_start");
// extern const uint8_t broker_ca_cert_pem_end[]   asm("_binary_broker_ca_cert_pem_end");
// extern const char ca_cert[];
// extern const char client_key[];
// extern const char client_cert[];
static const char *TAG = "[MQTT]";

esp_mqtt_client_handle_t mqtt_client = NULL;
bool mqtt_connected = false;
void handle_provision(uint8_t *data, int len);

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) 
    {
        case MQTT_EVENT_CONNECTED:
        {
            ESP_LOGI(TAG, "Connected to Broker");
            const char *prov_topic = "mesh/provisioning/hmac";
            esp_mqtt_client_subscribe(event->client, prov_topic, 1);
            ESP_LOGI(TAG, "Subscribed to %s", prov_topic);
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
            ESP_LOGE(TAG, "Free heap: %"PRIu32" bytes", esp_get_free_heap_size());
            break;
        }
        case MQTT_EVENT_DATA:
        {
            // 🔥 DEBUG FIRST
            printf("TOPIC: %.*s\n", event->topic_len, event->topic);
            printf("DATA : %.*s\n", event->data_len, event->data);
            printf("LEN  : %d\n", event->data_len);

            const char *prov_topic = "mesh/provisioning/hmac";

            if (event->topic_len == strlen(prov_topic) &&
                strncmp(event->topic, prov_topic, event->topic_len) == 0)
            {
                ESP_LOGI("[PROVISIONING]", "Provisioning message received");

                handle_provision((uint8_t*)event->data, event->data_len);
            }

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
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char client_id[24]; 
    snprintf(client_id, sizeof(client_id), "NODE_%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    ESP_LOGI(TAG, "Connecting with Client ID: %s", client_id);
    
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = uri,
        .credentials.username = user,
        .credentials.client_id = client_id,
        .credentials.authentication.password = pass,
        .credentials.authentication.certificate = client_cert,
        .credentials.authentication.key = client_key,
        .broker.verification.certificate = ca_cert,
        .broker.verification.skip_cert_common_name_check = true,
        .session.keepalive = 60,
    };
    uint8_t *cert = NULL;
    size_t cert_len = 0;

    if (load_ca_cert(&cert, &cert_len))
    {
        ESP_LOGI(TAG, "Using stored CA cert");
        mqtt_cfg.broker.verification.certificate = (const char *)ca_cert;
    } else {
        ESP_LOGI(TAG, "Using default CA cert");
        mqtt_cfg.broker.verification.certificate = ca_cert;
    }
    
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, 
                                   ESP_EVENT_ANY_ID, 
                                   mqtt_event_handler, 
                                   NULL
    );
    esp_mqtt_client_start(mqtt_client);
}


