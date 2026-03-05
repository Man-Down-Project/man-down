#include <Arduino.h>
#include <WiFiS3.h>
#include <WiFiSSLClient.h>
#include <PubSubClient.h>
#include "node.hpp"
#include "mqtt_client.hpp"
#include "config.hpp"
#include "../certs/ca_cert.hpp"
#include "../certs/client_cert.hpp"
#include "../certs/client_key.hpp"


WiFiSSLClient wifiClientSSL;
PubSubClient mqttClient(wifiClientSSL);

const char* broker = MQTT_BROKER;
const int port = 1883;

char topic[MAX_TOPIC_SIZE];

static unsigned long lastWifiAttempt = 0;
static unsigned long lastMQTTAttempt = 0;

#define WIFI_RETRY_INTERVAL 5000
#define MQTT_RETRY_INTERVAL 5000

void mqtt_init() {

    wifiClientSSL.setCACert(ca_cert);

    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    snprintf(topic, sizeof(topic), "mesh/node/%d/edge", NODE_ID);

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

    if (mqttClient.connect("node_client")) {
        Serial.println("MQTT connected");
    }else{
        Serial.print("MQTT failed: ");
        Serial.println(mqttClient.state());
    }
}

bool mqtt_publisher_edge_event(edge_event_t* pkt) {

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

void mqtt_loop(){

    mqtt_handle_wifi();
    mqtt_handle_connection();

    if (mqttClient.connected())
        mqttClient.loop();
}