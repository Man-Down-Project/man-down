#include <string.h>
#include <Crypto.h>
#include <SHA256.h>
#include <stddef.h>
#include "auth_node.hpp"

AuthNode authNode;
runtime_compare _seq_cache;

AuthNode::AuthNode() {

    for (int i = 0; i < MAX_APPROVED_EDGE; i++) {
        _ram_auth.device_whitelist[i] = EMPTY_ID;
    }
    
    memset(_seq_cache.device_id, EMPTY_ID, MAX_APPROVED_EDGE);
    memset(_seq_cache.last_seq, 0xFF, MAX_APPROVED_EDGE);

    memset(_ram_auth.auth.shared_key, 0, KEY_LEN);
    _ram_auth.auth.key_timestamp = 0;
}


bool AuthNode::isStorageEmpty(){

    if(_ram_auth.auth.key_timestamp != 0)
        return false;

    for(int i = 0; i < MAX_APPROVED_EDGE; i++){
        if(_ram_auth.device_whitelist[i] != EMPTY_ID){
            return false;
        }
    }
    return true;
}


void AuthNode::begin(uint8_t node_id) { 
    _node_id = node_id;

    loadAuthorizedEdges();

    if(isStorageEmpty()){
        Serial.println("Storage is empty.. Attempting provisioning...");
        //maby force a frist provisioning from mqtt topic here?
       
    }else{
        Serial.println("Storage already initialized");
    }
}



void AuthNode::loadAuthorizedEdges() { 
    EEPROM.get(0, _ram_auth);

    if(_ram_auth.magic != EEPROM_MAGIC || _ram_auth.version != EEPROM_VERSION){
        Serial.println("EEPROM invalid, resetting..");

        memset(&_ram_auth, 0, sizeof(_ram_auth));

        _ram_auth.magic = EEPROM_MAGIC;
        _ram_auth.version = EEPROM_VERSION;
        _ram_auth.auth.key_timestamp = 0;

        for(int i = 0; i < MAX_APPROVED_EDGE; i++){
            _ram_auth.device_whitelist[i] = EMPTY_ID;
        }
        EEPROM.put(0, _ram_auth);
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
    
    int idx = -1;

    for (int i = 0; i < MAX_APPROVED_EDGE; i++) { // detect duplication
        if (_ram_auth.device_whitelist[i] == device_id) {
            idx = i;
            break;
        }
    }
    
    if(idx == -1)
        return false;

    SHA256 sha;
    uint8_t full_hash[32];
    uint8_t computed_tag[AUTH_TAG_LEN];

    sha.resetHMAC(_ram_auth.auth.shared_key, KEY_LEN);
    sha.update((uint8_t*)pkt, offsetof(edge_event_t,  auth_tag));
    sha.finalizeHMAC(_ram_auth.auth.shared_key, KEY_LEN, full_hash, 32);

    memcpy(computed_tag, full_hash, AUTH_TAG_LEN);

    Serial.println("---- AUTH DEBUG ----");
    Serial.print("Received tag: ");
    for (int i = 0; i < AUTH_TAG_LEN; i++) {
        char buf[3]; // Buffer for 2 hex digits + null terminator
        sprintf(buf, "%02X", pkt->auth_tag[i]);
        Serial.print(buf);
    }
    Serial.println();

    Serial.print("Computed tag: ");
    for (int i = 0; i < AUTH_TAG_LEN; i++) {
        char buf[3];
        sprintf(buf, "%02X", computed_tag[i]);
        Serial.print(buf);
    }
    Serial.println();
    Serial.println("--------------------");
    // compare computed HMAC with received auth_tag
    if (!constTimeComp(pkt->auth_tag, computed_tag, AUTH_TAG_LEN)) {

        Serial.println("Auth tag mismatch!");

        return false;
    }

    uint8_t last = 0xFF;

    for(int i = 0; i < MAX_APPROVED_EDGE; i++){
        if(_seq_cache.device_id[i] == device_id){
            last = _seq_cache.last_seq[i];
            break;
        }
    }

    if(last != 0xFF && seq == last)
        return false;

    bool updated = false;
    
    for(int i = 0; i < MAX_APPROVED_EDGE; i++){
        if(_seq_cache.device_id[i] == device_id){
            _seq_cache.last_seq[i] = seq;
            updated = true;
            break;
        }
    }

    if(!updated){
        for(int i = 0; i < MAX_APPROVED_EDGE; i++){
            if(_seq_cache.device_id[i] == EMPTY_ID){
                _seq_cache.device_id[i] = device_id;
                _seq_cache.last_seq[i] = seq;
                break;
            }
        }
    }

    return true;
}


bool AuthNode::addDeviceToWhitelist(uint8_t device_id){
    for(int i = 0; i < MAX_APPROVED_EDGE; i++){
        if(_ram_auth.device_whitelist[i] == EMPTY_ID){
            _ram_auth.device_whitelist[i] = device_id;
            return true;
        }
    }

    return false; // List pace full
}


bool AuthNode::removeDeviceFromWhitelist(uint8_t device_id) {
    for(int i = 0; i < MAX_APPROVED_EDGE; i++){
        if(_ram_auth.device_whitelist[i] == device_id){
            _ram_auth.device_whitelist[i] = EMPTY_ID;
            return true;
        }
    }
    return false; //Device not found
}

int whitelistCompare(const uint8_t a[],int aLen, const uint8_t b[],int bLen, uint8_t out[]){

    int outIndex = 0;

    for(int i = 0; i < aLen; i++){
        
        if(a[i] == EMPTY_ID)
            continue;

        bool found = false;

        for(int j = 0; j < bLen; j++){
            if(b[j] == EMPTY_ID)
            continue;

            if(a[i] == b[j]){
                found = true;
                break;
            }
        }

        //duplicate protection from provision packet
        if(!found){ 
            bool existsInOutput = false;

            for(int k = 0; k < outIndex; k++){
                if(out[k] == a[i]){
                    existsInOutput = true;
                    break;
                }
            }
            //add new edge_id to add list
            if(!existsInOutput)
                out[outIndex++] = a[i];
        }
        
      
    }

    return outIndex;

}

int AuthNode::countWhitelist(){
    int count = 0;

    for(int i = 0; i < MAX_APPROVED_EDGE; i++){
        if(_ram_auth.device_whitelist[i] != EMPTY_ID){
            count++;
        }
    }
    return count;
}

void AuthNode::commitWhitelistIfChange(uint8_t* provisionedList, int provCount){

    uint8_t addList[MAX_APPROVED_EDGE];
    uint8_t removeList[MAX_APPROVED_EDGE];

    int curentCount = countWhitelist();

    int addCount = whitelistCompare(provisionedList, provCount, _ram_auth.device_whitelist, curentCount, addList);

    int removeCount = whitelistCompare(_ram_auth.device_whitelist, curentCount, provisionedList, provCount, removeList);

    int finalCount = curentCount -removeCount + addCount;

    if(finalCount > MAX_APPROVED_EDGE){
        Serial.println("Whitelist update rejected: risk of overflow");
        return;
    }
    
    for(int i = 0; i < removeCount; i++){
        removeDeviceFromWhitelist(removeList[i]);
    }    
    
    
    for(int i = 0; i < addCount; i++){
        addDeviceToWhitelist(addList[i]);
    }

    persistEEPROM();

}


void AuthNode::updateGlobalKey(uint8_t* new_key, uint32_t new_ts){
    if(!new_key) return;

    if(new_ts == 0){
        Serial.println("Invalid timestamp");
        return;
    }

    bool allZero = true;

    for(int i = 0; i < KEY_LEN; i++){
        if(new_key[i] != 0){
            allZero = false;
            break;
        }
    }

    if(allZero){
        Serial.println("Zero key rejected");
        return;
    }

    memcpy(_ram_auth.auth.shared_key, new_key, KEY_LEN);
    _ram_auth.auth.key_timestamp = new_ts;
    persistEEPROM();
}

bool hexCharToByte(char c, uint8_t &out){
    if(c >= '0' && c <= '9') out = c - '0';
    else if(c >= 'A' && c <= 'F') out = c -'A' +10;
    else if(c >= 'a' && c <= 'f') out = c -'a' +10;
    else return false;

    return true;
}

bool hexStringToByte(const char* str, uint8_t* out, size_t outLen){
    if(!str)
        return false;
    
    size_t len = strlen(str);
    if(len != outLen * 2)
        return false;

    for(int i = 0; i < outLen; i++){
        uint8_t hi, lo;

        if(!hexCharToByte(str[i * 2], hi))
            return false;

        if(!hexCharToByte(str[i * 2 + 1], lo))
            return false;

        out[i] = hi << 4 | lo;
    }

    return true;
}

void AuthNode::AuthEnrollmentFromSerial(){
    
    uint8_t buffer[21];
    uint8_t idx = 0;
    unsigned long start = millis();

    while(idx < sizeof(buffer)){
        if(Serial.available()){
            buffer[idx++] = Serial.read();
        }

        delay(1);

        if(millis() - start > 3000){
            Serial.println("Provision timeout");
            return;
        }
    }

    //packet parsing
    uint8_t device_id = buffer[0];
    uint8_t* new_key = &buffer[1];
    uint32_t new_ts =
    ((uint32_t)buffer[17] << 24) |
    ((uint32_t)buffer[18] << 16) |
    ((uint32_t)buffer[19] << 8)  |
    ((uint32_t)buffer[20]);

    if(device_id == EMPTY_ID){
        Serial.println("Invalid device id");
        return;
    }

    updateGlobalKey(new_key, new_ts);

    addDeviceToWhitelist(device_id);

    Serial.println("Serial provision complete");
}

void AuthNode::persistEEPROM() {
    EEPROM.put(0, _ram_auth);

}



//test start
/*
void AuthNode::begin(uint8_t node_id) { //fake for key test, shold be remove later

    _node_id = node_id;

    // Clear all authorized edges first
    for (int i = 0; i < MAX_APPROVED_EDGE; i++) {
        _ram_auth.device_whitelist[i].device_id = 0xFF;  // mark unused
        _ram_auth.device_whitelist[i].last_seq = 0xFF;
        _ram_auth.device_whitelist[i].key_timestamp = 1;
    }

    // ----- FAKE FOG PROVISIONING -----

    // We authorize ONE edge device (device_id = 1)
    _ram_auth.device_whitelist[0].device_id = 1;

    uint8_t fake_key[KEY_LEN] = {
        0x9A, 0x4F, 0x21, 0xC7,
        0x55, 0x13, 0xE8, 0x02,
        0x6D, 0xB9, 0x33, 0xA1,
        0x7C, 0x4D, 0x90, 0xEE
    };

    memcpy(_ram_auth.device_whitelist[0].shared_key, fake_key, KEY_LEN);

    _ram_auth.device_whitelist[0].last_seq = 0xFF;

    Serial.println("Edge provisioned (device_id = 1)");
}
*/
//test end
