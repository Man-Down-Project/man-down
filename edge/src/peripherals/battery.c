#include "battery.h"

static uint8_t battery_level = 94;

void battery_set(uint8_t level)
{
    battery_level = level;
}

uint8_t battery_get(void)
{
    return battery_level;
}