#include <Arduino.h>
#include "ble_gatt_peripheral.hpp"
#include "config.hpp"
#include "node.hpp"
#include "mqtt_client.hpp"
#include "auth_node.hpp"
#include "led_graphics.hpp"
#include "time_keeper.hpp"


bool systemReady = false;
bool timeInitialized = false;

void setup() {

  Serial.begin(115200);

  delay(100);

  node_init(NODE_ID); //init node struct
  authNode.begin(NODE_ID); //load auth edges from EEPROM

  ble_init("Node_1"); //start BLE peripherals
  mqtt_init(); //connect to wifi/mqtt broker

}

void loop() {
  
  mqtt_loop();
  
  while(systemReady == false){
     //keeps MQTT client alive
    
    if (!mqttClient.connected() && systemReady == false){
      return;
    }else if(mqttClient.connected() && systemReady == false){
      
      delay(1000);

      TimeInit();
      systemReady = true;
      break;
    }
  }
  
  ble_loop(authNode); // hadles BLE events and forwarding via MQTT
  
  //TimeSyncDaily();
}



