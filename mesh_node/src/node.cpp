#include <Arduino.h>
#include "node.hpp"
#include "config.hpp"

node_data_t my_node;

void node_init(uint8_t id){

    my_node.node_id = NODE_ID;
    my_node.parent_id = 0;
    my_node.node_depth = NODE_DEPTH;
    my_node.last_parent_heartbeat = millis();

    my_node.payload_len = 0;
}