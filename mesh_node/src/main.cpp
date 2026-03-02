#include <Arduino.h>
#include "ble_gatt_peripheral.hpp"
#include "config.hpp"
#include "node.hpp"
#include "mqtt_client.hpp"

void setup() {

  Serial.begin(115200);
  while (!Serial);

  node_init(NODE_ID);

  mqtt_init();
  ble_init("Node_1");
  
}

void loop() {
 
  ble_poll();
  mqtt_loop();

}



