#ifndef BLE_GAP_H
#define BLE_GAP_H

void start_scan(void);
void pairing_timeout_cb(TimerHandle_t xTimer);

void start_scan(void);
extern TimerHandle_t pairing_timer;

#endif

