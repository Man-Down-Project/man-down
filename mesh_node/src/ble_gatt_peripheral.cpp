#include <SHA256.h>
#include <string.h>
#include "ble_gatt_peripheral.hpp"
#include "config.hpp"
#include "mqtt_client.hpp"
#include "edge_event.hpp"
#include "auth_node.hpp"

BLEService meshService("12345678-1234-1234-1234-123456789abc"); // UUID

BLECharacteristic eventRX(
    "87654321-4321-4321-4321-cba987654321", 
    BLEWrite, 
    MAX_PAYLOAD
);

BLECharacteristic ackTX(
    "11111111-2222-3333-4444-555555555555",
    BLENotify,
    BLE_ACK_SIZE   
);

static uint8_t last_seq_per_edge[MAX_EDGE_DEVICES];

//AuthNode authNode;

void ble_init(const char* node_name){

    for (int i = 0; i < MAX_EDGE_DEVICES; i++){
    last_seq_per_edge[i] = 0xFF;
  }

  if (!BLE.begin()) {
    Serial.println("starting BLE module failed!");
    while (1);
  }

  BLE.setLocalName(node_name);
  BLE.setAdvertisedService(meshService);

  meshService.addCharacteristic(eventRX);
  meshService.addCharacteristic(ackTX);
  BLE.addService(meshService);

  BLE.advertise();
  Serial.println("Node BLE ready");
}


/*
void ble_poll(AuthNode &auth){ //test
   
    if (!eventRX.written()) return;

    Serial.println(sizeof(edge_event_t));

    uint8_t buf[sizeof(edge_event_t)];
    int len = eventRX.valueLength();
    eventRX.readValue(buf, len);
    
    if (len != sizeof (edge_event_t)) {
      
      Serial.print("Expected: ");
      Serial.println(sizeof(edge_event_t));

      Serial.print("Received: ");
      Serial.println(len);
      Serial.println("Invalid packet size");
      return;
    }

    edge_event_t pkt;
    memcpy(&pkt, buf, sizeof(pkt));

    uint8_t device_id = pkt.device_id;
    uint8_t seq = pkt.seq;

    if (device_id >= MAX_EDGE_DEVICES){
      Serial.println("Invalid device_id");
      return;
    }

    Serial.print("Packet from device: ");
    Serial.print(device_id);
    Serial.print(" seq: ");
    Serial.println(seq);

    bool valid = auth.validateEdge(&pkt);
    
  if (!valid) {
    Serial.println("Unauthorized ore duplicate event");
    uint8_t nack[BLE_ACK_SIZE] = { seq, 0x00 };
    ackTX.writeValue(nack, BLE_ACK_SIZE);
    return;
  }

  Serial.println("New event processed");

  bool ok = mqtt_publisher_edge_event(&pkt);
  if (!ok){
    Serial.println("MQTT publish failed");
    return;
  }

  uint8_t ack[2] = {seq, 0x01}; //0x01 = ok
  ackTX.writeValue(ack, 2);
  Serial.println("ACK sent");
}*/


void ble_poll(AuthNode &auth){
    BLE.poll();
    
    if (!eventRX.written()) return;
    //Serial.println(sizeof(edge_event_t));
    uint8_t buf[sizeof(edge_event_t)];
    int len = eventRX.valueLength();
    eventRX.readValue(buf, len);
    
    if (len != sizeof (edge_event_t)) {
      
      Serial.print("Expected: ");
      Serial.println(sizeof(edge_event_t));

      Serial.print("Received: ");
      Serial.println(len);
      Serial.println("Invalid packet size");
      return;
    }

    edge_event_t* pkt = (edge_event_t*)buf;
    uint8_t device_id = pkt->device_id;
    uint8_t seq = pkt->seq;

    if (device_id >= MAX_EDGE_DEVICES){
      Serial.println("Invalid device_id");
      return;
    }

    //uint8_t* shared_key = _authorized_edges[device_id].shared_key;

    Serial.print("Packet from device: ");
    Serial.print(device_id);
    Serial.print(" seq: ");
    Serial.println(seq);

    bool valid = auth.validateEdge(pkt);
    
  if (!valid) {
    Serial.println("Unauthorized or duplicate event");
    uint8_t nack[BLE_ACK_SIZE] = { seq, 0x00 };
    ackTX.writeValue(nack, BLE_ACK_SIZE);
    return;
  }

  uint8_t ack[2] = {seq, 0x01}; //0x01 = ok
  if (pkt->event_type == EVENT_HEARTBEAT){
    ackTX.writeValue(ack, 2);
  }

  Serial.println("New event processed");
  bool ok = mqtt_publisher_edge_event(pkt);
  if (pkt->event_type != EVENT_HEARTBEAT){
    if(ok){
      ackTX.writeValue(ack, 2);
      Serial.println("ACK sent");
      Serial.println("");

    }else{
      Serial.println("MQTT publish failed");
      return;
    }
  }


}


void ble_loop(AuthNode &auth){
  BLE.poll();
  BLEDevice central = BLE.central();

  if (central) {
    Serial.print("Edge connected: ");
    Serial.println(central.address());

    while (central.connected()){
      BLE.poll();
      ble_poll(auth);
    }
    Serial.println("Edge disconnected");

      if (!BLE.connected()) {
        BLE.advertise();
      }
  }
}
