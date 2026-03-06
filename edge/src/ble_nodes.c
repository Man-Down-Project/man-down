#include "edge_config.h"
#include "ble_nodes.h"
#include "ble_gap.h"

static const char *TAG = "BLE_NODE";

node_info_t nodes[MAX_NODES];

uint8_t get_node_id(const ble_addr_t *addr)
{
    return addr->val[5];
}

int find_node_index(const ble_addr_t *addr)
{
    for (int i = 0; i < MAX_NODES; i++)
    {
        if (nodes[i].valid &&
            ble_addr_cmp(&nodes[i].addr, addr) == 0)
        {
            return i;
        }
    }
    return -1;
}

void node_failure_tracker(int index)
{
    nodes[index].fail_count++;

    if(nodes[index].fail_count >= MAX_CONNECT_FAILS)
    {
        nodes[index].blacklist_timer =
            xTaskGetTickCount() + NODE_BLACKLIST_TIME;
        
        nodes[index].fail_count = 0;
        
        ESP_LOGW(TAG, "[NODE_%d|%02X] temporarily blacklisted",
                 index,
                 get_node_id(&nodes[index].addr));

        if (ble_gap_disc_active())
        {
            ble_gap_disc_cancel();
        }
        start_scan();
    }
}