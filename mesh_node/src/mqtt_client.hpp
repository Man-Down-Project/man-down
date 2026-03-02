#pragma once
#include <stdint.h>
#include "node.hpp"
#include "edge_event.hpp"

void mqtt_init();
bool mqtt_publisher_edge_event(edge_event_t* pkt);
void mqtt_loop();