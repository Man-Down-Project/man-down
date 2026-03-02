#include <Arduino.h>
#include <ArduinoBLE.h>

#define MAX_NEIGHBORS 5 
#define MAX_APPROVED_EDGE 10
#define MAX_PAYLOAD 8

#define EVENT_HEARTBEAT  0x00 // only need QoS 0

typedef struct {
    uint8_t seq;
    uint8_t device_id;
    uint8_t event_type;
    uint8_t location;
    uint8_t battery;
} edge_event_t;



/*void simulateEdgePacket() { // Fake BLE packet data
   
    uint8_t fakePacket[MAX_PAYLOAD] = {0xA5, 0x01, 0x02, 0x03};
    int len = 4;

    // Simulate eventChar
    Serial.print("Simulated packet received: ");
    for (int i = 0; i < len; i++) {
        Serial.print(fakePacket[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}*/



typedef struct{
  uint8_t node_id;
  uint8_t parent_id;
  uint8_t node_depth; //ex. if not == 1, sent to parent (never same depth if parent not dead). if switch it updates depth
  uint32_t last_parent_heartbeat;
  
  uint8_t approved_neighbors[MAX_NEIGHBORS]; // curent approved node devices
  uint8_t approved_neighbors_count;

  uint8_t neighbor_table[MAX_NEIGHBORS]; // secondary device routs
  uint8_t neighbor_count;

  uint8_t approved_list[MAX_APPROVED_EDGE]; // curent approved edge devices
  uint8_t approved_count;
  
  uint8_t payload[MAX_PAYLOAD];
  uint8_t payload_len;
}node_data_t;

node_data_t my_node;



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

uint8_t last_seq_per_edge[256];

void setup() {

  Serial.begin(115200);
  while (!Serial);
  
  for (int i = 0; i < 256; i++){
    last_seq_per_edge[i] = 0xFF;
  }
  /*
  my_node.node_id = 1;
  my_node.parent_id = 0;
  my_node.node_depth = 1;
  my_node.last_parent_heartbeat = millis();
  */

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


void loop() {
  BLE.poll();
  //simulateEdgePacket(); 

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
    
  }
}



