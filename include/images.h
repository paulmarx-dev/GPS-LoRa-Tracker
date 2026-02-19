#ifndef IMAGES_H
#define IMAGES_H

#include <Arduino.h>

const uint8_t activeSymbol[] PROGMEM = {
    B00000000,
    B00000000,
    B00011000,
    B00100100,
    B01000010,
    B01000010,
    B00100100,
    B00011000
};

const uint8_t inactiveSymbol[] PROGMEM = {
    B00000000,
    B00000000,
    B00000000,
    B00000000,
    B00011000,
    B00011000,
    B00000000,
    B00000000
};

const uint8_t wifiOn[] PROGMEM = {
    B10000001,
    B01010010,
    B01010100,
    B00111000,
    B00010000,
    B00010000,
    B00010000,
    B00010000
};

const uint8_t wifiOff[] PROGMEM = {
    B10000001,
    B01010010,
    B01010100,
    B00111000,
    B00111000,
    B01010100,
    B01010010,
    B00010000
};

#endif