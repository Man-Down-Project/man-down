#ifndef BLE_GATT_CLIENT_H
#define BLE_GATT_CLIENT_H

int gatt_svc_cb(uint16_t conn_handle,
                     const struct ble_gatt_error *error,
                     const struct ble_gatt_svc *service,
                     void *arg);


#endif