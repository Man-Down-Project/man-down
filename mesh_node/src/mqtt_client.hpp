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
bool mqtt_publisher_edge_event(const edge_event_t* pkt);
void mqtt_loop();