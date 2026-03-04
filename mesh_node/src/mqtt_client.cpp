#include <Arduino.h>
#include <WiFiS3.h>
#include <PubSubClient.h>

#include "node.hpp"
#include "mqtt_client.hpp"
#include "config.hpp"

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

const char* broker = MQTT_BROKER;
const int port = 1883;

char topic[MAX_TOPIC_SIZE];

void mqtt_init() {

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED){
        delay(500);
        Serial.print(".");
    }

    Serial.println("WiFi connected");

    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);

    while (!mqttClient.connected()) {
        Serial.print("Connecting MQTT..");

        if (mqttClient.connect("node_client")) {
            Serial.println("connected");
        }else{
            Serial.print("failed ");
            Serial.println(mqttClient.state());
            delay(2000);
        }
    }

    snprintf(topic, sizeof(topic), "mesh/node/%d/edge", my_node.node_id);
}

bool mqtt_publisher_edge_event(edge_event_t* pkt) {
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
    mqttClient.loop();
}