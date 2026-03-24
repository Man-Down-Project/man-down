#include <Arduino.h>
#include <WDT.h>
#include "ble_gatt_peripheral.hpp"
#include "config.hpp"
#include "node.hpp"
#include "mqtt_client.hpp"
#include "auth_node.hpp"
#include "led_graphics.hpp"
#include "time_keeper.hpp"

bool systemReady = false;
bool timeReady = false;
bool wdTimer = false;

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
    }else if(mqttClient.connected() && timeReady == false){
      
      delay(500);

      TimeInit();
      timeReady = true;

    }else if (timeReady && systemReady == false){
      if (WDT.begin(4096)){
        Serial.println("WD initialized");
        systemReady = true;
      }else{
        Serial.println("WD initialization retry...");
        return;
      }
    }
  }
  
  ble_loop(authNode); // hadles BLE events and forwarding via MQTT
  
  WDT.refresh();
  //TimeSyncDaily();
}



