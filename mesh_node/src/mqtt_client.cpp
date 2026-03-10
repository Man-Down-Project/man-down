#include <Arduino.h>
#include <queue>
#include "node.hpp"
#include "mqtt_client.hpp"
#include "config.hpp"
#include "../certs/ca_cert.hpp"

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

    if (mqttClient.connect(client_id, "", "")) {
        Serial.println("MQTT connected");

        snprintf(provision_topic, sizeof(provision_topic),
                "mesh/node/%d/provision/ca", NODE_ID);

        mqttClient.subscribe(provision_topic);
        Serial.println("Subscribed to provisioning topic...");

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
    return mqttClient.publish(
        topic,
        (uint8_t*)&msg,
        sizeof(msg)
    );
}

void mqtt_provision_handeling(const char* topic, byte* payload, unsigned int length) {

    if (strcmp(topic, "mesh/provision/ca") != 0)
        return;
    Serial.println("Provisioning: New CA received");

    char newCA[length +1];
    memcpy(newCA, payload, length);
    newCA[length] = '\0';

    Serial.println("Rebooting to apply new CA...");
    delay(500);
    NVIC_SystemReset();
    
}

void mqtt_callback(char* topic, byte* payload, unsigned int length){
    mqtt_provision_handeling(topic, payload, length);

    //can add other message handler here....
}

bool mqtt_forward_event(const edge_event_out* msg, uint8_t original_node_id) {
    if (!mqttClient.connected())
    return false;

    char forward_topic[MAX_TOPIC_SIZE];
    snprintf(forward_topic, sizeof(forward_topic), "mesh/node/%d/edge", original_node_id);

    return mqttClient.publish(forward_topic, (uint8_t*)msg, sizeof(edge_event_out));
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