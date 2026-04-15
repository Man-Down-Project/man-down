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

    bool addDeviceToWhitelist(const uint8_t mac[MAC_LEN]); // Update id whitelist

    bool removeDeviceFromWhitelist(const uint8_t mac[MAC_LEN]);

    void AuthEnrollmentFromSerial(); //Provision one full edge regestration packet

    void updateGlobalKey(uint8_t* new_key, uint32_t new_ts);

    void commitWhitelistIfChange(const uint8_t provisionedList[][MAC_LEN], int provCount);

    int countWhitelist();

    void persistEEPROM(); // store authorized edge from RAM to EEPROM

    bool isStorageEmpty();

    const uint8_t* getWhiteListEntry(int i) const;
    
    const uint8_t* getSharedKey() const;


private:
    uint8_t _node_id;
    eeprom_global_auth _ram_auth;
    bool validateHMAC(edge_event_t* pkt, uint8_t* key);
};

int whitelistCompare(const uint8_t a[][MAC_LEN],int aLen, const uint8_t b[][MAC_LEN],int bLen, uint8_t out[][MAC_LEN]);
bool constTimeComp(const uint8_t* a, const uint8_t* b, size_t len);
bool hexCharToByte(char c, uint8_t &out);
bool hexStringToByte(const char* str, uint8_t* out, size_t outLen);
void compute_hmac16(const uint8_t* key, size_t key_len, const uint8_t *data, size_t data_len, uint8_t* out16);

extern AuthNode authNode;
extern eeprom_global_auth _eeprom;
extern runtime_compare _seq_cache;
