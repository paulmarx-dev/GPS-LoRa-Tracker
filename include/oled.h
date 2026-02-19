#ifndef OLED_H
#define OLED_H

#define LH 12 // Line Height for UI elements, used for layout calculations

#include <Arduino.h>
#include "HT_SSD1306Wire.h"
#include "HT_DisplayUi.h"
#include "images.h"
#include <TimeLib.h>    // for clock display
#include "gps.h"        // for GPS data access
#include "button.h"     // for button state access
#include <WiFi.h>       // for WiFi status access

extern DisplayUi ui;

/**
 * Initialize UI framework and OLED display
 */
void uiInit();

/**
 * Enable Vext (external power supply for OLED)
 */
void oledVextON();

/**
 * Disable Vext (external power supply for OLED)
 */
void oledVextOFF();

/**
 * Initialize OLED display
 */
void oledInit();

/**
 * Display searching indicator with animated progress bar
 * @param progress Progress value (0-99)
 */
void oledDisplaySearching(int progress);

/**
 * Display GPS data on OLED screen
 * @param hasTime Whether GPS has valid time data
 * @param hasLoc Whether GPS has valid location data
 * @param hour GPS hour (0-23)
 * @param minute GPS minute (0-59)
 * @param second GPS second (0-59)
 * @param centisecond GPS centisecond (0-99)
 * @param latitude GPS latitude coordinate
 * @param longitude GPS longitude coordinate
 * @param antennaOpen Whether antenna is open/shorted
 * @param antennaStatusValid Whether antenna status is recent
 */
void oledDisplayUpdate(bool hasTime, bool hasLoc,
                       uint8_t hour, uint8_t minute, uint8_t second, uint8_t centisecond,
                       double latitude, double longitude,
                       bool antennaOpen, bool antennaStatusValid);

/**
 * Battery status frame for UI display
 */
void BatteryFrame(ScreenDisplay *display, DisplayUiState* state, int16_t x, int16_t y);

#endif
