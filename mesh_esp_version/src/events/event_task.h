#ifndef EVENT_TASK_H
#define EVENT_TASK_H

#include <stdint.h>
#include "mqtt_client.h"

#include "system_events.h"
#include "node.h"

void event_task_init();
void system_event_post(system_event_type_t type, uint32_t data);
extern bool wifi_connected_globally;
extern esp_mqtt_client_handle_t mqtt_client;
extern edge_event_out outgoing;
#endif