#include <Arduino.h>
#include "ble_gatt_peripheral.hpp"
#include "config.hpp"
#include "node.hpp"
#include "mqtt_client.hpp"
#include "auth_node.hpp"




void setup() {

  Serial.begin(115200);
  //while (!Serial); //can block if usb not connected

  node_init(NODE_ID); //init node struct
  authNode.begin(NODE_ID); //load auth edges from EEPROM

  mqtt_init(); //conect to wifi/mqtt broker
  ble_init("Node_1"); //start BLE peripherals
  
}

void loop() {
 
  ble_poll(authNode); // hadles BLE events and forwarding via MQTT
  mqtt_loop(); //keeps MQTT client alive

}



