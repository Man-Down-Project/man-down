#pragma once

#include <ArduinoGraphics.h>
#include <Arduino_LED_Matrix.h>

extern ArduinoLEDMatrix matrix;


enum systemState{
    STATE_BOOT,
    STATE_BLE_CONNECTING,
    STATE_BLE_CONNECTED,
    STATE_WIFI_CONNECTING,
    STATE_WIFI_CONNECTED,
    STATE_MQTT_CONNECTING,
    STATE_MQTT_CONNECTED,
    STATE_MESSAGE_SENT,
    STATE_ERROR
};

extern uint8_t wifi_symbol[96];
extern uint8_t mqtt_image[96];
extern uint8_t TLS_image[96];
extern uint8_t error_image[96];
extern uint8_t success_image[96];
extern uint8_t message_image[96];

extern systemState currentState;

void led_indication(uint8_t bitmap[96]);
void update_leds();
