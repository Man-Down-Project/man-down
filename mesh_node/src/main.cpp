#include <Arduino.h>
#include <ArduinoBLE.h>

#define MAX_NEIGHBORS 5 
#define MAX_APPROVED_EDGE 10
#define MAX_PAYLOAD 8

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
BLECharacteristic eventChar(
    "87654321-4321-4321-4321-cba987654321", 
    BLEWrite | BLEWriteWithoutResponse, 
    MAX_PAYLOAD
);


void simulateEdgePacket() { // Fake BLE packet data
   
    uint8_t fakePacket[MAX_PAYLOAD] = {0xA5, 0x01, 0x02, 0x03};
    int len = 4;

    // Simulate eventChar
    Serial.print("Simulated packet received: ");
    for (int i = 0; i < len; i++) {
        Serial.print(fakePacket[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}

void setup() {

  Serial.begin(115200);
  while (!Serial);
  Serial.println("Hello");

  my_node.node_id = 1;
  my_node.parent_id = 0;
  my_node.node_depth = 1;
  my_node.last_parent_heartbeat = millis();

  /*
  if (!BLE.begin()) {
    Serial.println("starting BLE module failed!");
    while (1);
  }

  BLE.setLocalName("Node_1");
  BLE.setAdvertisedService(meshService);

  meshService.addCharacteristic(eventChar);
  BLE.addService(meshService);

  BLE.advertise();
  Serial.println("Node BLE ready");
  */
}

void loop() {
  //BLE.poll();
  simulateEdgePacket(); 

  if (eventChar.written()){
    int len = eventChar.valueLength();
    uint8_t buf[MAX_PAYLOAD];
    eventChar.readValue(buf, len);
    
    Serial.print("Received packet");
    for (int i=0; i<len; i++) Serial.print(buf[i], HEX);
    Serial.println();
  }
    delay(500);
}



