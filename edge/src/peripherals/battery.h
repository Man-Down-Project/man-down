#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>

void battery_set(uint8_t level);
uint8_t battery_get(void);

#endif