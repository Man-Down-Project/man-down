#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include "config.hpp"
#include "edge_event.hpp"

struct eeprom_edge_t {
    uint8_t device_id;
    uint8_t shared_key[KEY_LEN];
    uint8_t last_seq;
    uint32_t key_timestamp; //for key update purposes (mabeY shold liv in fog?)
}; 

class AuthNode{
public:
    AuthNode();

    void begin(uint8_t node_id);

    void loadAuthorizedEdges(); //loads edges from EEPROM to RAM

    bool validateEdge(edge_event_t* pkt); // Device authorize and duplicate check

    bool updateEdgeKey(uint8_t device_id, uint8_t* new_key, uint32_t new_ts); // Update device key

    bool removeEdge(uint8_t device_id); // remove edge from autorized list

    void persistEdge(uint8_t index); // store authorized edge from RAM to EEPROM

private:
    uint8_t _node_id;
    eeprom_edge_t _authorized_edge[MAX_APPROVED_EDGE];
    bool validateHMAC(edge_event_t* pkt, uint8_t* key);
};

extern AuthNode authNode;

