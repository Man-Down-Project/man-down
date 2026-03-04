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


//test start
void AuthNode::begin(uint8_t node_id) { //fake for key test, remove later

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
void AuthNode::begin(uint8_t node_id) {
    _node_id = node_id;
    loadAuthorizedEdges();
}
*/


void AuthNode::loadAuthorizedEdges() {
    for (int i = 0; i < MAX_APPROVED_EDGE; i++) {
        int addr = EEPROM_START + i * sizeof(eeprom_edge_t); 
        EEPROM.get(addr, _authorized_edge[i]);
    }
}

//test start
bool AuthNode::validateEdge(edge_event_t* pkt) {
    if (!pkt) return false;

    uint8_t device_id = pkt->device_id;
    uint8_t seq = pkt->seq;

    for (int i = 0; i < MAX_APPROVED_EDGE; i++) { // detect duplication
        if (_authorized_edge[i].device_id == device_id) {

            if (_authorized_edge[i].key_timestamp == 0) {
                return false; // not provisioned
            }

            if (_authorized_edge[i].last_seq == seq) {
                return false; // duplicate sequence
            }

            // --- HMAC verification ---
            SHA256 sha;
            uint8_t computed_tag[AUTH_TAG_LEN];

            sha.resetHMAC(_authorized_edge[i].shared_key, KEY_LEN);
            sha.update((uint8_t*)pkt, sizeof(edge_event_t) - AUTH_TAG_LEN);
            sha.finalizeHMAC(_authorized_edge[i].shared_key, KEY_LEN, computed_tag, AUTH_TAG_LEN);

            // compare computed HMAC with received auth_tag
            if (memcmp(pkt->auth_tag, computed_tag, AUTH_TAG_LEN) != 0) {
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
}//test end





/* REmove later maby?
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

            if (memcmp(pkt->auth_tag, computed_tag, AUTH_TAG_LEN) != 0) {
                Serial.println("Auth tah mismatch!");
                return false;
            }
            
            _authorized_edge[i].last_seq = seq; //update in RAM
            return true;
        }
    }
    return false; //unathourized
}
*/



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