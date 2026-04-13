#ifndef EDGE_EVENT_H
#define EDGE_EVENT_H

#include <stdint.h>

void edge_trigger_event(uint8_t event_type, uint8_t battery);
extern uint8_t current_node_id;
#endif