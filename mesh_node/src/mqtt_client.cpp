#include <Arduino.h>
#include <queue>
#include "node.hpp"
#include "mqtt_client.hpp"
#include "config.hpp"
#include "../certs/ca_cert.hpp"
#include "time_keeper.hpp"
#include "auth_node.hpp"
#include "boot.hpp"
#include <SHA256.h>

struct childEvent_t {
    edge_event_out msg;
    uint8_t node_id; //original sender
};

std::queue<childEvent_t> childEventQueue;

WiFiSSLClient wifiClientSSL;
PubSubClient mqttClient(wifiClientSSL);
char topic [MAX_TOPIC_SIZE];
char provision_topic[64];

static unsigned long lastWifiAttempt = 0;
static unsigned long lastMQTTAttempt = 0;
static const char* active_ca = ca_cert;

char broker_ip[16];
int broker_port = MQTT_PORT;
int current_parent = 0;


void mqtt_init() {

    wifiClientSSL.setCACert(ca_cert);

    mqttClient.setCallback(mqtt_callback);

    mqttClient.setBufferSize(256);

    //choosing broker
    if (NODE_DEPTH == 1){
        strncpy(broker_ip, MQTT_BROKER, sizeof(broker_ip)-1);
        broker_ip[sizeof(broker_ip)-1] = '\0';
    }else{
        strncpy(broker_ip, BACKUP_PARENTS[current_parent], sizeof(broker_ip)-1);
        broker_ip[sizeof(broker_ip)-1] = '\0';
    }

    mqttClient.setServer(broker_ip, broker_port);
    snprintf(topic, sizeof(topic), "mesh/node/%d/edge", NODE_ID);

    Serial.print("MQTT broker ip: ");
    Serial.println(broker_ip);
    Serial.print("MQTT topic: ");
    Serial.println(topic);
}

void mqtt_handle_wifi() {

    if (WiFi.status() == WL_CONNECTED) 
        return;

    if (millis() - lastWifiAttempt < WIFI_RETRY_INTERVAL)
        return;
    
    lastWifiAttempt = millis();
    Serial.println("Connecting Wifi...");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    if (WiFi.status() == WL_CONNECTED) 
        Serial.println("Wifi connected");
}

void mqtt_handle_connection() {

    if (WiFi.status() != WL_CONNECTED) 
        return;

    if (mqttClient.connected())
        return;

    if (millis() - lastMQTTAttempt < MQTT_RETRY_INTERVAL)
        return;

    lastMQTTAttempt = millis();
    Serial.println("Connecting MQTT...");

    char client_id[32];
    snprintf(client_id, sizeof(client_id), "node_%d", NODE_ID);

    if (mqttClient.connect(client_id, "mesh_user", "dev")) {
        Serial.println("MQTT connected");

        const char* provision_topic = "mesh/provisioning/#";

        mqttClient.subscribe(provision_topic);
        Serial.println("Attempting to subscribe to provisioning topic...");

    }else{
        Serial.print("MQTT failed: ");
        Serial.println(mqttClient.state());

        if (NODE_DEPTH > 1){
        current_parent++;

            if (current_parent >= MAX_BACKUP_PARENTS)
                current_parent = 0;

            strncpy(broker_ip, BACKUP_PARENTS[current_parent], sizeof(broker_ip)-1);
            broker_ip[sizeof(broker_ip)-1] = '\0';

            mqttClient.setServer(broker_ip, broker_port);

            Serial.print("Switching parent to: ");
            Serial.println(broker_ip);
        }
    }
}

bool mqtt_publisher_edge_event(const edge_event_t* pkt) {

    if (!mqttClient.connected())
        return false;


    edge_event_out msg = {
        pkt->device_id,
        pkt->event_type,
        pkt->location,
        pkt->battery,
        pkt->seq
        
    };

    msg.timestamp = GetTimeStamp();
    
    uint8_t data[7] = {
        msg.device_id,
        msg.event_type,
        msg.location,
        msg.battery,
        msg.seq,
        (uint8_t)(msg.timestamp >> 8),
        (uint8_t)(msg.timestamp & 0XFF)
    };

    compute_hmac16(
        _eeprom.auth.shared_key,
        KEY_LEN,
        data,
        sizeof(data),
        msg.hmac
    );
    

    return mqttClient.publish(
        topic,
        (uint8_t*)&msg,
        sizeof(msg)
    );
}

bool mqtt_forward_event(const edge_event_out* msg, uint8_t original_node_id) {
    if (!mqttClient.connected())
    return false;

    char forward_topic[MAX_TOPIC_SIZE];
    snprintf(forward_topic, sizeof(forward_topic), "mesh/node/%d/edge", original_node_id);

    return mqttClient.publish(forward_topic, (uint8_t*)msg, sizeof(edge_event_out));
}

void mqtt_callback(char* topic, byte* payload, unsigned int length){
    mqtt_provision_handeling(topic, payload, length);
}

void mqtt_provision_handeling(const char* topic, byte* payload, unsigned int length) {

    if(strcmp(topic, "mesh/provisioning/edgeid") == 0){
        Serial.println("Initiating Whitelist provisioning");
        handle_edgeid_provision(payload, length);

    }else if(strcmp(topic, "mesh/provisioning/hmac") == 0){
        Serial.println("Initiating HMAC key provisioning");
        handle_hmac_provision(payload, length);
    /*    
    }else if (strcmp(topic, "mesh/provisioning/ca") == 0){
        Serial.println("Initiating CA provisioning");
        handle_ca_provision(payload, length);
    */
    }else{
        return;
    }

        
}



void handle_edgeid_provision(byte* payload, unsigned int length){
    char buffer[128];

    if(length >= sizeof(buffer))
        return;

    memcpy(buffer, payload, length);
    buffer[length] = '\0';

    uint8_t provisioned[MAX_APPROVED_EDGE];

    for(int i = 0; i < MAX_APPROVED_EDGE; i++){
        provisioned[i] = EMPTY_ID;
    }

    int count = 0;

    char* token = strtok(buffer, ",");

    while(token != nullptr && count < MAX_APPROVED_EDGE){
        
        int id = atoi(token);

        if(id > 0 && id < 255){
            provisioned[count++] = (uint8_t)id;
        }

        token = strtok(nullptr, ",");
    }

    authNode.commitWhitelistIfChange(provisioned, count);

}

void handle_hmac_provision(byte* payload, unsigned int len){
    
    char buffer[41];

    if(len != 40 || len > 40){
        Serial.println("Invalid HMAC length");
        return;
    }
    
    memcpy(buffer, payload, 40);

    char keyStr[33];
    char tsStr[9];

    Serial.print("HMAC topic len: ");
    Serial.println(len);

    Serial.print("Payload: ");
    for (unsigned int i = 0; i < len; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();

    memcpy(keyStr, buffer, 32);
    keyStr[32] = '\0';

    Serial.print("Key: ");
    Serial.println(keyStr);

    memcpy(tsStr, buffer + 32, 8);
    tsStr[8] = '\0';

    Serial.print("TS: ");
    Serial.println(tsStr);


    for(int i = 0; i < 8; i++){
        if(tsStr[i] < '0' || tsStr[i] > '9'){
            Serial.println("Invalid timestamp format");
            return;
        } 
    }

    uint32_t timestamp = strtoul(tsStr, nullptr, 10);

    uint8_t key[16];

    if(!hexStringToByte(keyStr, key, 16)){
        Serial.println("Invalid HMAC key format");
        return;
    }

    authNode.updateGlobalKey(key, timestamp);

    Serial.println("HMAC DONE");

}


void mqtt_loop(){

    mqtt_handle_wifi();
    mqtt_handle_connection();

    if (!mqttClient.connected())
        return;

    mqttClient.loop();

    while (!childEventQueue.empty()){
        childEvent_t pkt = childEventQueue.front();
        childEventQueue.pop();
        
        if (!mqtt_forward_event(&pkt.msg, pkt.node_id)){
            childEventQueue.push(pkt);
            break;
        }
    }
}