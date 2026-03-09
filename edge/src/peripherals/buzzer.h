#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

typedef enum 
{
    BUZZER_NONE,
    BUZZER_FALL,
    BUZZER_GAS,
    BUZZER_LOW_BATTERY,
    BUZZER_ERROR,

} buzzer_pattern_t;

void buzzer_init();
void buzzer_play(buzzer_pattern_t pattern);
void buzzer_stop();

#endif