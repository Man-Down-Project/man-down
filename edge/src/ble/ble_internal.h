#ifndef BLE_INTERNAL_H
#define BLE_INTERNAL_H

#include "host/ble_gap.h"
#include "config/edge_config.h"

#define MAX_NODES 10

typedef struct {
    ble_addr_t addr;
    int8_t     rssi;
    bool       valid;
    uint32_t   last_seen;
    uint8_t    fail_count;
    uint32_t   blacklist_timer;
} node_info_t;

//-----------Node Table-------------//
extern node_info_t nodes[MAX_NODES];

// --------Connection State---------//
extern uint16_t current_conn_handle;
extern ble_addr_t current_peer_addr;
extern int current_conn_rssi;
extern uint8_t own_addr_type;
extern int last_connect_index;
extern ble_state_t ble_state;

#endif