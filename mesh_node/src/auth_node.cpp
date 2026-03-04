#include <string.h>
#include <Crypto.h>
#include <SHA256.h>
#include "auth_node.hpp"

AuthNode authNode;

AuthNode::AuthNode() {

    //eeprom_edge_t _authorized_edges[MAX_APPROVED_EDGE];

    for (int i = 0; i < MAX_APPROVED_EDGE; i++) {
        _authorized_edge[i].device_id = 0xFF;
        _authorized_edge[i].last_seq = 0;
        _authorized_edge[i].key_timestamp = 0;
        memset(_authorized_edge[i].shared_key, 0, KEY_LEN);
    }
}

void AuthNode::begin(uint8_t node_id) {
    _node_id = node_id;
    //EEPROM.begin(MAX_APPROVED_EDGE * sizeof(eeprom_edge_t) + EEPROM_START); // wrong for uno R4?
    loadAuthorizedEdges();
}

void AuthNode::loadAuthorizedEdges() {
    for (int i = 0; i < MAX_APPROVED_EDGE; i++) {
        int addr = EEPROM_START + i * sizeof(eeprom_edge_t); 
        EEPROM.get(addr, _authorized_edge[i]);
    }
}

bool AuthNode::validateEdge(edge_event_t* pkt) {
    if (!pkt) return false;

    uint8_t device_id = pkt->device_id;
    uint8_t seq = pkt->seq;

    for (int i = 0; i < MAX_APPROVED_EDGE; i++) { //detect duplication
        if (_authorized_edge[i].device_id == device_id) {

            if (_authorized_edge[i].key_timestamp == 0){
                return false;
            }

            if (_authorized_edge[i].last_seq == seq) {
                return false;
            }

            //HMAC verification
            SHA256 sha;
            uint8_t computed_tag[AUTH_TAG_LEN];

            sha.resetHMAC(_authorized_edge[i].shared_key, KEY_LEN);
            sha.update((uint8_t*)pkt, sizeof(edge_event_t) - AUTH_TAG_LEN);
            sha.finalizeHMAC(_authorized_edge[i].shared_key, KEY_LEN, computed_tag, AUTH_TAG_LEN);

            if (memcpy(pkt->auth_tag, computed_tag, AUTH_TAG_LEN) != 0) {
                Serial.println("Auth tah mismatch!");
                return false;
            }
            
            _authorized_edge[i].last_seq = seq; //update in RAM
            return true;
        }
    }
    return false; //unathourized
}

bool AuthNode::updateEdgeKey(uint8_t device_id, uint8_t* new_key, uint32_t new_ts) {
    if (!new_key) return false;

    for (int i = 0; i < MAX_APPROVED_EDGE; i++) {
        if (_authorized_edge[i].device_id == device_id) {
            memcpy(_authorized_edge[i].shared_key, new_key, KEY_LEN);
            _authorized_edge[i].last_seq = 0;
            _authorized_edge[i].key_timestamp = new_ts;
            persistEdge(i);
            return true;
        }
    }
    return false; //device not found
}

bool AuthNode::removeEdge(uint8_t device_id) {
    for (int i = 0; i < MAX_APPROVED_EDGE; i++) {
        if (_authorized_edge[i].device_id == device_id) {
            _authorized_edge[i].device_id = 0xFF;
            _authorized_edge[i].last_seq = 0;
            _authorized_edge[i].key_timestamp = 0;
            memset(_authorized_edge[i].shared_key, 0, KEY_LEN);
            persistEdge(i);
            return true;
        }
    }
    return false;
}

void AuthNode::persistEdge(uint8_t index) {
    if (index >= MAX_APPROVED_EDGE) return;

    int addr = EEPROM_START + index * sizeof(eeprom_edge_t);
    EEPROM.put(addr, _authorized_edge[index]);
}