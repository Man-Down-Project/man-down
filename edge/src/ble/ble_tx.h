#ifndef BLE_TX_H
#define BLE_TX_H


#include "stdbool.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

#include "config/edge_config.h"


typedef struct {
    edge_event_t event;
} ble_tx_msg_t;

extern QueueHandle_t ble_tx_queue;
extern TimerHandle_t heartbeat_timer;
extern volatile bool gatt_busy;
extern bool tx_packet_pending;
extern uint8_t sequence_counter;

void ble_send_event(const edge_event_t *event);
void ble_tx_task(void *arg);
void heartbeat_timer_cb(TimerHandle_t xTimer);

#endif