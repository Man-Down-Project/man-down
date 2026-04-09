#include <Arduino.h>
#include <WDT.h>
#include "ble_gatt_peripheral.hpp"
#include "config.hpp"
#include "node.hpp"
#include "mqtt_client.hpp"
#include "auth_node.hpp"
#include "led_graphics.hpp"
#include "time_keeper.hpp"
#include "boot.hpp"

void setup() {

  Serial.begin(115200);

  delay(100);

  node_init(NODE_ID); //init node struct
  authNode.begin(NODE_ID); //load auth edges from EEPROM

  ble_init("Node_1"); //start BLE peripherals
  mqtt_init(); //connect to wifi/mqtt broker

  boot_init();

}

void loop() {
  
  if(systemState == RESTART_PENDING){
    Serial.println("System restarting...");
    delay(500);
    NVIC_SystemReset();
  }

  mqtt_loop();
  boot_loop();
  
  if(systemState == RUNNING){
    ble_loop(authNode);
    TimeSyncDaily();
    WDT.refresh();
  }
}



