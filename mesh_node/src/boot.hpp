#pragma once
#include <cstdint>
#include "config.hpp"


enum SystemState : uint8_t {
    BOOT,
    WAIT_MQTT,
    INIT_TIME,
    INIT_WDT,
    RUNNING
};

void boot_init();
void boot_loop();

extern SystemState systemState;
