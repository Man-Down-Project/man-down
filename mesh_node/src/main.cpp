#include <Arduino.h>
#include "ble_gatt_peripheral.hpp"

#define MAX_NEIGHBORS 5 
#define MAX_APPROVED_EDGE 10

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


void setup() {

  Serial.begin(115200);
  while (!Serial);

  ble_init("Node_1");
  
}


void loop() {
 
  ble_poll();

}



