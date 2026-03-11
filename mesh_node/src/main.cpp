#include <Arduino.h>
#include "ble_gatt_peripheral.hpp"
#include "config.hpp"
#include "node.hpp"
#include "mqtt_client.hpp"
#include "auth_node.hpp"
#include "led_graphics.hpp"



void setup() {

  Serial.begin(115200);
  //matrix.begin();
  
  delay(100);
  //update_leds();
  node_init(NODE_ID); //init node struct
  authNode.begin(NODE_ID); //load auth edges from EEPROM

  ble_init("Node_1"); //start BLE peripherals
  mqtt_init(); //connect to wifi/mqtt broker
}

void loop() {
  //update_leds();
  ble_poll(authNode); // hadles BLE events and forwarding via MQTT
  mqtt_loop(); //keeps MQTT client alive
}



