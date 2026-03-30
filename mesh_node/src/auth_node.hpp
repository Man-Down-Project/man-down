#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include "config.hpp"
#include "edge_event.hpp"

#define EEPROM_MAGIC 0xDEADBEEF
#define EEPROM_VERSION 1
#define EMPTY_ID 0xFF


struct __attribute__ ((packed)) global_auth{
    uint8_t shared_key[KEY_LEN];
    uint32_t key_timestamp; //format MMDDHHM
};

struct __attribute__ ((packed)) eeprom_edge_t {
    uint32_t magic;
    uint8_t version;

    uint8_t device_id[MAX_APPROVED_EDGE];
    global_auth hmac;
};  

class AuthNode{
public:
    AuthNode();

    void begin(uint8_t node_id);

    void loadAuthorizedEdges(); //loads edges from EEPROM to RAM

    bool validateEdge(edge_event_t* pkt); // Device authorize and duplicate check

    bool updateEdgeKey(uint8_t device_id, uint8_t* new_key, uint32_t new_ts); // Update device key

    bool removeEdge(uint8_t device_id); // remove edge from autorized list

    void edgeEnrollmentFromSerial(); //Provision full edge regestration packet

    void persistEdge(uint8_t index); // store authorized edge from RAM to EEPROM

    bool isStorageEmpty();


private:
    uint8_t _node_id;
    eeprom_edge_t _authorized_edge[MAX_APPROVED_EDGE];
    bool validateHMAC(edge_event_t* pkt, uint8_t* key);
};

bool constTimeComp(const uint8_t* a, const uint8_t* b, size_t len);

extern AuthNode authNode;

