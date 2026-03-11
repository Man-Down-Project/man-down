#include "led_graphics.hpp"

ArduinoLEDMatrix matrix;

systemState currentState = STATE_BOOT;
systemState lastState = STATE_BOOT;

uint8_t template_pic[96] = {
    0,0,0,0,0,0,0,0,0,0,0,0,  // top arc
    0,0,0,0,0,0,0,0,0,0,0,0,  // second arc
    0,0,0,0,0,0,0,0,0,0,0,0,  // third arc
    0,0,0,0,0,0,0,0,0,0,0,0,  // small inner arc
    0,0,0,0,0,0,0,0,0,0,0,0,  // repeat inner arc
    0,0,0,0,0,0,0,0,0,0,0,0,  // dot
    0,0,0,0,0,0,0,0,0,0,0,0,  // empty row
    0,0,0,0,0,0,0,0,0,0,0,0   // empty row
};

uint8_t boot_image[96] = {
    0,0,0,0,0,0,0,0,0,0,0,0,  // top arc
    0,0,0,0,0,1,0,0,0,0,0,0,  // second arc
    0,0,0,0,1,1,1,0,0,0,0,0,  // third arc
    0,0,0,1,0,1,0,1,0,0,0,0,  // small inner arc
    0,0,1,0,0,0,0,0,1,0,0,0,  // repeat inner arc
    0,0,1,0,0,0,0,0,1,0,0,0,  // dot
    0,0,0,1,0,0,0,1,0,0,0,0,  // empty row
    0,0,0,0,1,1,1,0,0,0,0,0   // empty row
};

uint8_t ble_image[96] = {
    0,0,0,0,0,0,1,0,0,0,0,0,  // top arc
    0,0,0,0,1,0,1,1,0,0,0,0,  // second arc
    0,0,0,0,0,1,1,0,1,0,0,0,  // third arc
    0,0,0,0,0,0,1,1,0,0,0,0,  // small inner arc
    0,0,0,0,0,1,1,1,0,0,0,0,  // repeat inner arc
    0,0,0,0,1,0,1,0,1,0,0,0,  // dot
    0,0,0,0,0,0,1,1,0,0,0,0,  // empty row
    0,0,0,0,0,0,1,0,0,0,0,0   // empty row
};

uint8_t wifi_symbol[96] = {
    0,0,1,1,1,1,1,1,1,1,0,0,  // top arc
    0,1,0,0,0,0,0,0,0,0,1,0,  // second arc
    1,0,0,1,1,1,1,1,1,0,0,1,  // third arc
    0,0,1,0,0,0,0,0,0,1,0,0,  // small inner arc
    0,1,0,0,1,1,1,1,0,0,1,0,  // repeat inner arc
    0,0,0,1,0,0,0,0,1,0,0,0,  // dot
    0,0,0,0,0,1,1,0,0,0,0,0,  // empty row
    0,0,0,0,0,0,0,0,0,0,0,0   // empty row
};

uint8_t message_image[96] = {
    1,1,1,1,1,1,1,1,1,1,1,1,  // top border
    1,1,0,0,0,0,0,0,0,0,1,1,  // upper part
    1,0,1,0,0,0,0,0,0,1,0,1,  // upper part
    1,0,0,1,0,0,0,0,1,0,0,1,  // envelope flap
    1,0,0,0,1,0,0,1,0,0,0,1,  // envelope flap
    1,0,0,0,0,1,1,0,0,0,0,1,  // envelope flap
    1,0,0,0,0,0,0,0,0,0,0,1,  // bottom part
    1,1,1,1,1,1,1,1,1,1,1,1   // bottom border
};


uint8_t mqtt_image[96] = {
    0,0,0,0,1,1,1,0,0,0,0,0,  // top arc
    1,1,1,1,0,1,1,0,0,0,0,0,  // second arc
    0,0,0,0,1,0,1,0,0,0,0,0,  // third arc
    1,1,1,0,0,1,0,0,0,0,0,0,  // small inner arc
    0,0,0,1,0,0,1,0,0,0,0,0,  // repeat inner arc
    1,1,0,0,1,0,1,0,0,0,0,0,  // dot
    1,1,1,0,1,0,1,0,0,0,0,0,  // empty row
    1,1,1,0,1,0,1,0,0,0,0,0   // empty row
};

uint8_t TLS_image[96] = {
    0,0,0,0,0,0,0,0,0,0,0,0,  // top arc
    0,0,0,0,0,0,0,0,0,0,0,0,  // second arc
    0,0,0,0,0,1,1,0,0,0,0,0,  // third arc
    0,0,0,0,1,0,0,1,0,0,0,0,  // small inner arc
    0,0,0,1,1,1,1,1,1,0,0,0,  // repeat inner arc
    0,0,0,1,0,0,0,0,1,0,0,0,  // dot
    0,0,0,1,0,0,0,0,1,0,0,0,  // empty row
    0,0,0,1,1,1,1,1,1,0,0,0   // empty row
};

uint8_t error_image[96] = {
    0,0,1,0,0,0,0,0,0,1,0,0,  // top arc
    0,0,0,1,0,0,0,0,1,0,0,0,  // second arc
    0,0,0,0,1,0,0,1,0,0,0,0,  // third arc
    0,0,0,0,0,1,1,0,0,0,0,0,  // small inner arc
    0,0,0,0,0,1,1,0,0,0,0,0,  // repeat inner arc
    0,0,0,0,1,0,0,1,0,0,0,0,  // dot
    0,0,0,1,0,0,0,0,1,0,0,0,  // empty row
    0,0,1,0,0,0,0,0,0,1,0,0   // empty row
};

uint8_t success_image[96] = {
    0,0,0,0,0,0,0,0,0,0,0,0,  // top arc
    0,0,0,0,0,0,0,0,0,0,0,0,  // second arc
    0,0,0,0,0,0,0,0,0,1,0,0,  // third arc
    0,0,0,0,0,0,0,0,1,0,0,0,  // small inner arc
    0,0,0,0,0,0,0,1,0,0,0,0,  // repeat inner arc
    0,0,1,0,0,0,1,0,0,0,0,0,  // dot
    0,0,0,1,0,1,0,0,0,0,0,0,  // empty row
    0,0,0,0,1,0,0,0,0,0,0,0   // empty row
};


void led_indication(uint8_t bitmap[96]){
    
    static uint8_t *lastBitmap = nullptr; 

    if (lastBitmap == bitmap)
        return;

    lastBitmap = bitmap;
    matrix.loadPixels(bitmap,96);
}

void update_leds(){
    

    static systemState lastState = STATE_BOOT;
    
    static unsigned long lastUpdate = 0;
    const unsigned long UPDATE_INTERVAL = 100;

    static unsigned long lastBlink = 0;
    static bool ledOn = false;
    
    const int BLINK_INTERVAL = 100;

    if (millis() - lastUpdate < UPDATE_INTERVAL)
        return;
    lastUpdate = millis();

  
    if (millis() - lastBlink > BLINK_INTERVAL){
        lastBlink = millis();
        ledOn = !ledOn;
    }

    if (lastState == currentState && !currentState == STATE_BOOT)
        return;

    lastState = currentState;

    switch(currentState){
        case STATE_BOOT:
            led_indication(boot_image);
            break;

        case STATE_BLE_CONNECTING:
            if (ledOn)
                led_indication(ble_image);
            else
                matrix.clear();
            break;

        case STATE_BLE_CONNECTED:
            led_indication(ble_image);
            break;

        case STATE_WIFI_CONNECTING:
            if (ledOn)
                led_indication(wifi_symbol);
            else
                matrix.clear();
            break;

        case STATE_WIFI_CONNECTED:
            led_indication(wifi_symbol);
            break;

        case STATE_MQTT_CONNECTING:
            if (ledOn)
                led_indication(mqtt_image);
            else
                matrix.clear();
            break;

        case STATE_MQTT_CONNECTED:
            led_indication(mqtt_image);
            break;

        case STATE_MESSAGE_SENT:
            led_indication(message_image);
            break;    

        case STATE_ERROR:
            led_indication(error_image);
            break;

        default:
            break;
    };

}