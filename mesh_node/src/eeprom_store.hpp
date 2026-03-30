#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include "config.hpp"

#define EEPROM_MAGIC 0xDEADBEEF
#define EEPROM_VERSION 1
#define EMPTY_ID 0xFF


struct __attribute__ ((packed)) global_auth{
    uint8_t shared_key[KEY_LEN];
    uint32_t key_timestamp; //format MMDDHHM
};

struct __attribute__ ((packed)) eeprom_global_auth{
    uint32_t magic;
    uint8_t version;

    uint8_t device_whitelist[MAX_APPROVED_EDGE];
    global_auth auth;
};  

extern eeprom_global_auth _eeprom;

