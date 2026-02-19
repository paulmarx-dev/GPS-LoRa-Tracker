#ifndef GPS_H
#define GPS_H

#include <Arduino.h>
#include <TinyGPSPlus.h>


// UART Pins (RX/TX communication with GPS module)
#define GNSS_RX 39
#define GNSS_TX 38
// Power/Control Pins
#define VGNSS_CTRL 34   // active LOW
#define GNSS_WAKE 40    // active HIGH
#define GNSS_RST 42     // active LOW
#define GNSS_PPS 41     // optional, not used in this code. Needed for precise timing applications

// External GPS object (defined in gps.cpp)
extern TinyGPSPlus GPS;

// External antenna status variables (defined in gps.cpp)
extern bool antennaOpen;
extern uint32_t lastAntennaMsg;

/**
 * Initialize GPS module and UART serial communication
 */
void gpsInit();

/**
 * Read and parse incoming GPS data from UART
 * Call this regularly in loop() to process GPS sentences
 */
void gpsUpdate();

/**
 * Check if GPS has valid time data
 */
bool gpsHasTime();

/**
 * Check if GPS has valid location data
 */
bool gpsHasLocation();

/**
 * Check if the last antenna status message is recent enough to be valid
 */
bool gpsAntennaStatusValid();

/**
 * Return true for winter time and false for summer time (EU DST rules).
 */
bool gpsIsWinterTime();

#endif
