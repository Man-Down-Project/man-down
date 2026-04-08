#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include "config.hpp"
#include "edge_event.hpp"
#include "eeprom_store.hpp"

class AuthNode{
public:
    AuthNode();

    void begin(uint8_t node_id);

    void loadAuthorizedEdges(); //loads edges from EEPROM to RAM

    bool validateEdge(edge_event_t* pkt); // Device authorize and duplicate check

    bool addDeviceToWhitelist(uint8_t device_id); // Update id whitelist

    bool removeDeviceFromWhitelist(uint8_t device_id);

    void AuthEnrollmentFromSerial(); //Provision one full edge regestration packet

    void updateGlobalKey(uint8_t* new_key, uint32_t new_ts);

    void commitWhitelistIfChange(uint8_t* provisionedList, int provCount);

    int countWhitelist();

    void persistEEPROM(); // store authorized edge from RAM to EEPROM

    bool isStorageEmpty();


private:
    uint8_t _node_id;
    eeprom_global_auth _ram_auth;
    bool validateHMAC(edge_event_t* pkt, uint8_t* key);
};

int whitelistCompare(const uint8_t a[],int aLen, const uint8_t b[],int bLen, uint8_t out[]);
bool constTimeComp(const uint8_t* a, const uint8_t* b, size_t len);
bool hexCharToByte(char c, uint8_t &out);
bool hexStringToByte(const char* str, uint8_t* out, size_t outLen);

extern AuthNode authNode;
extern eeprom_global_auth _eeprom;
extern runtime_compare _seq_cache;
