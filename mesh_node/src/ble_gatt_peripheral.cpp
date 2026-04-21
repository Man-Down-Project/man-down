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

static uint8_t session_packet_count = 0;

//noun handeling and validation
/*
static uint8_t seen_tags[MAX_DATA_PACKETS_PER_SESSION][AUTH_TAG_LEN];
static uint8_t seen_count = 0;
*/
//end


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

void ble_poll(AuthNode &auth){
    BLE.poll();
    
    if (!eventRX.written()) return;
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
    const uint8_t* mac = pkt->device_id.mac;
    uint8_t seq = pkt->seq;


    Serial.print("Packet from device: ");
    Serial.print("MAC: ");
    for(int i = 0; i < 6; i++){
      Serial.print(mac[i], HEX);
      if(i < 5)
        Serial.print(":");
    }
    Serial.println();

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
      Serial.println("Event type: Heartbeat");
  }

  Serial.println("New event processed");
  bool ok = mqtt_publisher_edge_event(pkt);
  if (pkt->event_type != EVENT_HEARTBEAT){
    if(ok){
      ackTX.writeValue(ack, 2);

      Serial.print("Event type: ");

      switch (pkt->event_type)
      {
      case EVENT_FALLARM:
        Serial.println("Fallarm");
        break;

      case EVENT_GASLARM:
        Serial.println("Gaslarm");
        break;

      default:
        Serial.println("Unknown event type");
        break;
      }

      Serial.println("ACK sent");
      Serial.println("");

    }else{
      Serial.println("MQTT publish failed");
      return;
    }
  }


}

bool isMacInWhitelist(const uint8_t* mac, AuthNode &auth){
  //
  Serial.print("Checking MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.print(mac[i], HEX);
      if (i < 5) Serial.print(":");
  }
  Serial.println();
  //
  
  for(int i = 0; i < MAX_APPROVED_EDGE; i++){
    const uint8_t* entry = auth.getWhiteListEntry(i);

    //
    Serial.print("Entry ");
    Serial.print(i);  
    Serial.print(": ");
    //
    //
    for (int j = 0; j < 6; j++) {
      Serial.print(entry[j], HEX);
      if (j < 5) Serial.print(":");
    }
    Serial.println();
    //
    
    bool isEmpty = true;

    for(int j = 0; j < 6; j++){
      if(entry[j] != EMPTY_ID){
        isEmpty = false;
        break;
      }
    }
    if (isEmpty) continue;

    if(memcmp(mac, entry, MAC_LEN) == 0){
      Serial.println("Match found");
      return true;
    }
  }
  Serial.println("No match found");
  return false;
}
bool parseMac(const String &addr, uint8_t mac[6]) {
  int values[6];

  if (sscanf(addr.c_str(),
    "%x:%x:%x:%x:%x:%x",
    &values[0], &values[1], &values[2],
    &values[3], &values[4], &values[5]) != 6) {
      return false;
  }

  for (int i = 0; i < 6; i++) {
    mac[i] = (uint8_t)values[i]; 
  }

  return true;
    
}

void ble_loop(AuthNode &auth){ 
  BLE.poll();
  BLEDevice central = BLE.central();

  if(central) {
    Serial.print("Edge connected: ");
    Serial.println(central.address());

    uint8_t mac[6];

    String addr = central.address();
    addr.trim();

    if(!parseMac(addr, mac)){
      Serial.println("Mac parse failed");
      central.disconnect();
      return;
    }

    if(!isMacInWhitelist(mac, auth)){
      Serial.println("Unauthorized device- disconnecting");
      central.disconnect();
      return;
    }

    Serial.println("AUthorized edge");
  

    while (central.connected()){
      BLE.poll();
      ble_poll(auth);
    }
    Serial.println("Edge disconnected");
    Serial.println("");

      
    BLE.advertise();
      
  }
}


//noun handeling and validation
/*
void ble_loop(AuthNode &auth){
  BLE.poll();
  BLEDevice central = BLE.central();

  if (central) {
    Serial.print("Edge connected: ");
    Serial.println(central.address());

    session_packet_count = 0;
    seen_count = 0;

    while (central.connected()){
      BLE.poll();
      ble_poll(auth);
    }
    Serial.println("Edge disconnected");
    Serial.println("");

      if (!BLE.connected()) {
        BLE.advertise();
      }
  }
}
  */
//end
