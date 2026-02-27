#include <Arduino.h>

#define MAX_NEIGHBORS 5 
#define MAX_APPROVED_EDGE 10
#define MAX_PAYLOAD 8

def struct{
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

// put function declarations here:
//int myFunction(int, int);

void setup() {
  // put your setup code here, to run once:
  //int result = myFunction(2, 3);
  Serial.begin(115200);

  my_node.node_id = 1;
  my_node.parent_id = 0;
  my_node.node_depth = 1;
  my_node.last_parent_heartbeat_ms = millis();
  my_node.node_id = 1;
  my_node.parent_id = 0;
  my_node.node_depth = 1;
  my_node.last_parent_heartbeat_ms = millis();
  my_node.payload[0] = 0xA5;
  my_node.payload_len = 1;


  Serial.println("Node initialized!");
}

void loop() {
  // put your main code here, to run repeatedly:
    my_node.last_parent_heartbeat_ms = millis();
    delay(1000);
}

// put function definitions here:
//int myFunction(int x, int y) {
  //return x + y;
//}


