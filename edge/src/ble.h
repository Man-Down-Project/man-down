#ifndef BLE_H
#define BLE_H

#include "edge_config.h"
//RTOS task queue struct
extern ble_state_t ble_state;
typedef struct {
    edge_event_t event;
} ble_tx_msg_t;
void ble_send_event(const edge_event_t *event);

void ble_init();
uint16_t ble_get_conn_handle();
void ble_on_ready(uint16_t conn_handle);

#endif