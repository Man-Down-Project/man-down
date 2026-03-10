#include <Arduino.h>
#include <queue>
#include "node.hpp"
#include "mqtt_client.hpp"
#include "config.hpp"


struct childEvent_t {
    edge_event_out msg;
    uint8_t node_id; //original sender
};

std::queue<childEvent_t> childEventQueue;

WiFiSSLClient wifiClientSSL;
PubSubClient mqttClient(wifiClientSSL);
char topic [MAX_TOPIC_SIZE];

static unsigned long lastWifiAttempt = 0;
static unsigned long lastMQTTAttempt = 0;

char broker_ip[16];
int broker_port = MQTT_PORT;
int current_parent = 0;


void mqtt_init() {

    wifiClientSSL.setCACert(ca_cert);

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

bool mqtt_forward_event(const edge_event_out* msg, uint8_t original_node_id){
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