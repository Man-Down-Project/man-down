#ifndef EVENT_TASK_H
#define EVENT_TASK_H

#include <stdint.h>
#include "system_events.h"

void event_task_init();
void system_event_post(system_event_type_t type, uint32_t data);

#endif