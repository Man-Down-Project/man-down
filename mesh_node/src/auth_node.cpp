#include <string.h>
#include <Crypto.h>
#include <SHA256.h>
#include <stddef.h>
#include "auth_node.hpp"

AuthNode authNode;

AuthNode::AuthNode() {

    for (int i = 0; i < MAX_APPROVED_EDGE; i++) {
        _authorized_edge[i].device_id = 0xFF;
        _authorized_edge[i].last_seq = 0;
        _authorized_edge[i].key_timestamp = 0;
        memset(_authorized_edge[i].shared_key, 0, KEY_LEN);
    }
}


//test start
void AuthNode::begin(uint8_t node_id) { //fake for key test, shold be remove later

    _node_id = node_id;

    // Clear all authorized edges first
    for (int i = 0; i < MAX_APPROVED_EDGE; i++) {
        _authorized_edge[i].device_id = 0xFF;  // mark unused
        _authorized_edge[i].last_seq = 0xFF;
        _authorized_edge[i].key_timestamp = 1;
    }

    // ----- FAKE FOG PROVISIONING -----

    // We authorize ONE edge device (device_id = 1)
    _authorized_edge[0].device_id = 1;

    uint8_t fake_key[KEY_LEN] = {
        0x9A, 0x4F, 0x21, 0xC7,
        0x55, 0x13, 0xE8, 0x02,
        0x6D, 0xB9, 0x33, 0xA1,
        0x7C, 0x4D, 0x90, 0xEE
    };

    memcpy(_authorized_edge[0].shared_key, fake_key, KEY_LEN);

    _authorized_edge[0].last_seq = 0xFF;

    Serial.println("Fake edge provisioned (device_id = 1)");
}
//test end


/*
void AuthNode::begin(uint8_t node_id) {  //This is the live code
    _node_id = node_id;
    loadAuthorizedEdges();
}
*/


void AuthNode::loadAuthorizedEdges() { //live code
    for (int i = 0; i < MAX_APPROVED_EDGE; i++) {
        int addr = EEPROM_START + i * sizeof(eeprom_edge_t); 
        EEPROM.get(addr, _authorized_edge[i]);
    }
}

bool constTimeComp(const uint8_t* a, const uint8_t* b, size_t len){
    uint8_t diff = 0;

    for (size_t i = 0; i < len; i++) {
        diff |= a[i] ^ b[i];
    }

    return diff == 0;
}

bool AuthNode::validateEdge(edge_event_t* pkt) {
    if (!pkt) return false;

    uint8_t device_id = pkt->device_id;
    uint8_t seq = pkt->seq;

    for (int i = 0; i < MAX_APPROVED_EDGE; i++) { // detect duplication
        if (_authorized_edge[i].device_id == device_id) {

            if (_authorized_edge[i].key_timestamp == 0) {
                return false; // not provisioned
            }
            
            if (_authorized_edge[i].last_seq == seq) // just for demo, remove later
                return false;
            /*
            if (seq <= _authorized_edge[i].last_seq) //real code when provitioning correct
                return false; // duplicate sequence
            */
            _authorized_edge[i].last_seq = seq;
            


            // --- HMAC verification ---
            SHA256 sha;
            uint8_t full_hash[32];
            uint8_t computed_tag[AUTH_TAG_LEN];

            sha.resetHMAC(_authorized_edge[i].shared_key, KEY_LEN);
            sha.update((uint8_t*)pkt, offsetof(edge_event_t,  auth_tag));
            sha.finalizeHMAC(_authorized_edge[i].shared_key, KEY_LEN, full_hash, AUTH_TAG_LEN);
            memcpy(computed_tag, full_hash, AUTH_TAG_LEN);
            // compare computed HMAC with received auth_tag
            if (!constTimeComp(pkt->auth_tag, computed_tag, AUTH_TAG_LEN)) {
                Serial.println("Auth tag mismatch!");

                Serial.print("Computed: ");
                for (int j = 0; j < AUTH_TAG_LEN; j++) {
                    Serial.print(computed_tag[j], HEX); Serial.print(" ");
                }
                Serial.println();

                Serial.print("Received: ");
                for (int j = 0; j < AUTH_TAG_LEN; j++) {
                    Serial.print(pkt->auth_tag[j], HEX); Serial.print(" ");
                }
                Serial.println();

                return false;
            }

            _authorized_edge[i].last_seq = seq; // update sequence in RAM
            return true;
        }
    }

    return false; // unauthorized device
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