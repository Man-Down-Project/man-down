#ifndef BLE_GAP_H
#define BLE_GAP_H

void start_scan(void);
void pairing_timeout_cb(TimerHandle_t xTimer);

void start_scan(void);
extern TimerHandle_t pairing_timer;
bool ble_tx_pending(void);
void ble_connect(void);
void ble_disconnect(void);
void ble_gap_connect_to(const ble_addr_t *addr);



#endif

