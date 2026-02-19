#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>

#define BATTERY_PIN 1          // GPIO1 (ADC1_CH0) - battery voltage sense
#define ADC_CTRL_PIN 37        // GPIO37 - ADC control (must be HIGH to enable reading)
#define BATTERY_ADC_BITS 12    // 12-bit ADC on ESP32
#define BATTERY_MIN_MV 3300    // 3.3V = 0%
#define BATTERY_MAX_MV 4200    // 4.2V = 100%
#define BATTERY_DIVIDER 4.9    // voltage divider: VBAT = VADC * (100+390)/100 = VADC * 4.9

/**
 * Initialize battery monitoring: ADC pin and charging detect GPIO
 */
void batteryInit();

/**
 * Update battery readings (voltage, charging state, estimate)
 * Call periodically (e.g., every 10 seconds)
 */
void batteryUpdate();

/**
 * Get current battery percentage (0-100)
 * @return Battery level as percentage
 */
uint8_t getBatteryPercent();

/**
 * Check if device is charging
 * @return true if voltage is rising (inferred from trend), false otherwise
 */
bool isCharging();

/**
 * Get estimated charging time in minutes
 * @return Minutes to reach 100%, 0 if not charging or invalid
 */
uint16_t getChargingTimeEstimateMin();

/**
 * Get current battery voltage in millivolts
 * @return Battery voltage in mV
 */
uint16_t getBatteryVoltageMV();

#endif // BATTERY_H
