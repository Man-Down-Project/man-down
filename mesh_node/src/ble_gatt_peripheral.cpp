#include "ble_gatt_peripheral.hpp"


BLEService meshService("12345678-1234-1234-1234-123456789abc"); // UUID

BLECharacteristic eventRX(
    "87654321-4321-4321-4321-cba987654321", 
    BLEWrite, 
    MAX_PAYLOAD
);

BLECharacteristic ackTX(
    "11111111-2222-3333-4444-555555555555",
    BLENotify,
    2   
);

static uint8_t last_seq_per_edge[256];

void ble_init(const char* node_name){

    for (int i = 0; i < 256; i++){
    last_seq_per_edge[i] = 0xFF;
  }

  if (!BLE.begin()) {
    Serial.println("starting BLE module failed!");
    while (1);
  }

  BLE.setLocalName("Node_1");
  BLE.setAdvertisedService(meshService);

  meshService.addCharacteristic(eventRX);
  meshService.addCharacteristic(ackTX);
  BLE.addService(meshService);

  BLE.advertise();
  Serial.println("Node BLE ready");
}

void ble_poll(){
    BLE.poll();
      if (eventRX.written()){
    
    uint8_t buf[MAX_PAYLOAD];
    int len = eventRX.valueLength();
    eventRX.readValue(buf, len);
    
    if (len < sizeof (edge_event_t)) {
      Serial.println("Invalid packet size");
      return;
    }

    edge_event_t* pkt = (edge_event_t*)buf;

    uint8_t device_id = pkt->device_id;
    uint8_t seq = pkt->seq;

    if (device_id >=256){
      Serial.println("Invalid device_id");
      return;
    }

    Serial.print("Packet from device: ");
    Serial.print(device_id);
    Serial.print("seq: ");
    Serial.println(seq);

    if (pkt->event_type == EVENT_HEARTBEAT){
      Serial.println("Heartbeat");
      
    }else{

      if (last_seq_per_edge[device_id] == seq){
        Serial.println("Duplicate detected, re-ACK");
      }else{
        last_seq_per_edge[device_id] = seq;
        Serial.println("New event processed"); 
      }
    }
    //send upstream (towards fog) if ok continue down to send ACK
      
    uint8_t ack[2] = {seq, 0x01}; //0x01 = ok
    ackTX.writeValue(ack, 2);
    Serial.println("ACK sent");
    Serial.println(pkt->device_id);
  }
}