#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "host/ble_gap.h"

void provisioning_init(void);
bool provisioning_is_active(void);
void provisioning_on_scan_match(const ble_addr_t *addr);
void provisioning_on_connected(uint16_t conn_handle);
void provisioning_handle_rx(const uint8_t *data, size_t len);
