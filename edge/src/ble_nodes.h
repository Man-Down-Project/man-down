#ifndef BLE_NODES_H
#define BLE_NODES_H

#include "ble_internal.h"

int find_node_index(const ble_addr_t *addr);
void node_failure_tracker(int index);
uint8_t get_node_id(const ble_addr_t *addr);

#endif