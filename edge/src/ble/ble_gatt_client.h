#ifndef BLE_GATT_CLIENT_H
#define BLE_GATT_CLIENT_H

#include "host/ble_hs.h"
#include "host/ble_gatt.h"

#include "config/edge_config.h"

int gatt_svc_cb(uint16_t conn_handle,
                     const struct ble_gatt_error *error,
                     const struct ble_gatt_svc *service,
                     void *arg);
void gatt_client_reset();
void gatt_client_init();
int gatt_send_event(uint16_t conn_handle, edge_event_t *event);
const ble_uuid_t *gatt_get_service_uuid();
void gatt_set_handles(uint16_t svc_start,
                      uint16_t svc_end,
                      uint16_t event_handle,
                      uint16_t ack_handle,
                      uint16_t cccd_handle);
void gatt_enable_notifications(uint16_t conn_handle, uint16_t cccd_handle);
const ble_uuid_t *gatt_get_provisioning_service_uuid(void);
#endif