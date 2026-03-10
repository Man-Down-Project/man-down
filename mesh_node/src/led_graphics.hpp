#pragma once

#include <ArduinoGraphics.h>
#include <Arduino_LED_Matrix.h>

ArduinoLEDMatrix matrix;

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

uint8_t envelope[96] = {
    1,1,1,1,1,1,1,1,1,1,1,1,  // top border
    1,1,0,0,0,0,0,0,0,0,1,1,  // upper part
    1,0,1,0,0,0,0,0,0,1,0,1,  // upper part
    1,0,0,1,0,0,0,0,1,0,0,1,  // envelope flap
    1,0,0,0,1,0,0,1,0,0,0,1,  // envelope flap
    1,0,0,0,0,1,1,0,0,0,0,1,  // envelope flap
    1,0,0,0,0,0,0,0,0,0,0,1,  // bottom part
    1,1,1,1,1,1,1,1,1,1,1,1   // bottom border
};

uint8_t middleFinger[8][12] = {
  {0,0,0,0,1,1,0,0,0,0,0,0},
  {0,0,0,0,1,1,0,0,0,0,0,0},
  {0,0,0,0,1,1,0,0,0,0,0,0},
  {0,0,1,1,1,1,1,1,0,0,0,0},
  {0,1,1,1,1,1,1,1,1,0,0,0},
  {0,1,1,1,1,1,1,1,1,0,0,0},
  {0,0,1,1,1,1,1,1,0,0,0,0},
  {0,0,0,1,1,1,1,0,0,0,0,0}
};

void led_indication(uint8_t bitmap[96]){
    matrix.beginDraw();
    matrix.loadPixels(bitmap,96);
    delay(5000);
    matrix.endDraw();

}

