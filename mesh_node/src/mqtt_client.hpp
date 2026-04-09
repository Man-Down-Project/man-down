#pragma once
#include <WiFiS3.h>
#include <WiFiSSLClient.h>
#include <PubSubClient.h>
#include <stdint.h>
#include "node.hpp"
#include "edge_event.hpp"
#include "config.hpp"

extern WiFiSSLClient wifiClientSSL;
extern PubSubClient mqttClient;
extern char topic [MAX_TOPIC_SIZE];

void mqtt_init();
void mqtt_provision_handeling(const char* topic, byte* payload, unsigned int length);
void handle_edgeid_provision(byte* payload, unsigned int length);
void handle_hmac_provision(byte* payload, unsigned int len);
void handle_ca_provision(byte* payload, unsigned int len);
bool mqtt_publisher_edge_event(const edge_event_t* pkt);
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void mqtt_loop();