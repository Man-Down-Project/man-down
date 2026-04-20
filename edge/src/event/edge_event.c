
#include "ble/ble_tx.h"
#include "security/auth.h"
#include "config/edge_config.h"
#include "ble/ble_internal.h"
#include "ble/ble_nodes.h"
#include "peripherals/battery.h"
#include "peripherals/buzzer.h"
#include "event/edge_event.h"
#include "esp_mac.h"

void edge_trigger_event(uint8_t event_type, uint8_t battery)
{
    edge_event_t event;

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);

    memcpy(event.device_id, mac, 6);
    event.event_type = event_type;
    event.event_location = current_node_id;
    event.battery_status = battery_get();
    event.seq = sequence_counter++;

    memset(event.auth_tag, 0, AUTH_TAG_LEN);

    ble_send_event(&event);
}
