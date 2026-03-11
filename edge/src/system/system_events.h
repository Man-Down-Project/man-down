#ifndef SYSTEM_EVENTS_H
#define SYSTEM_EVENTS_H

#include <stdint.h>

typedef enum
{
    EVENT_NONE = 0,

    EVENT_BUTTON_SHORT,
    EVENT_BUTTON_LONG,
    EVENT_BUTTON_DOUBLE,
    EVENT_BUTTON_RESET,
    EVENT_BUTTON_POWER,

    EVENT_FALL_ALARM,
    EVENT_GAS_ALARM,

    EVENT_BLE_CONNECTED,
    EVENT_BLE_DISCONNECTED

} system_event_type_t;

typedef struct
{
    system_event_type_t type;
    uint32_t data;
} system_event_t;


#endif
