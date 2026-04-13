
#include "ble/ble_tx.h"
#include "security/auth.h"
#include "config/edge_config.h"
#include "ble/ble_internal.h"
#include "peripherals/battery.h"
#include "peripherals/buzzer.h"

void edge_trigger_event(uint8_t event_type, uint8_t battery)
{
    edge_pkt_t event;

    event.device_id = DEVICE_ID;
    event.event_type = event_type;
    event.event_location = 0;
    event.battery_status = battery_get();
    event.seq = sequence_counter++;

    memset(event.auth_tag, 0, AUTH_TAG_LEN);

    generate_auth_tag((uint8_t*)&event,
                      sizeof(edge_event_t) - AUTH_TAG_LEN,
                      event.auth_tag
    );
    ble_send_event(&event);
}
