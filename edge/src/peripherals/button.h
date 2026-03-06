#pragma once

#include <stdbool.h>

typedef enum {
    BUTTON_NONE,
    BUTTON_SHORT,
    BUTTON_DOUBLE,
    BUTTON_LONG,
    BUTTON_RESET,
    BUTTON_POWER

} button_event_t;

//button_event_t button_update();
void button_init();