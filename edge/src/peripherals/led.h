#ifndef LED_H
#define LED_H

#include <stdint.h>

typedef enum {
    RGB_OFF = 0,
    RGB_RED,
    RGB_BLUE,
    RGB_GREEN,
    RGB_YELLOW,
    RGB_MAGENTA,
    RGB_WHITE,
    RGB_CYAN
} rgb_color_t;

typedef enum {
    LED_MODE_OFF,
    LED_MODE_SOLID,
    LED_MODE_BLINK,
    LED_MODE_BREATHE,
    LED_MODE_PULSE
} led_mode_t;

typedef enum {
    LED_PRIO_LOW = 0,
    LED_PRIO_MEDIUM,
    LED_PRIO_HIGH
} led_priority_t;

void led_init(void);
void led_set(rgb_color_t color, led_mode_t mode, led_priority_t prio);
void led_off(void);
void led_release(led_priority_t prio);


#endif