#include "battery.h"

// Battery state variables
static uint16_t batteryVoltageMV = 4200;  // in millivolts
static uint8_t batteryPercent = 100;
static bool charging = false;
static bool lastCharging = false;  // Track state changes

// Charging estimation: track voltage over time
static uint16_t lastVoltageMV = 4200;
static uint32_t lastUpdateMs = 0;
static uint16_t chargingTimeEstimateMin = 0;

void batteryInit() {
  pinMode(ADC_CTRL_PIN, OUTPUT);
  digitalWrite(ADC_CTRL_PIN, HIGH);  // Enable ADC reading by pulling GPIO37 HIGH
  
  // GPIO1 is already ADC-capable, default 12-bit resolution (0-4095 = 0-2600mV attenuation)
  // Note: GPIO18 is OLED_SCL, no hardware charging detect available on V4
}

void batteryUpdate() {
  // Read battery voltage from ADC
  int rawAdc = analogRead(BATTERY_PIN);
  
  // Convert ADC (0-4095 for 12-bit) to voltage in mV
  // ADC reads the divided voltage VADC_IN1 = VBAT * 100 / (100+390)
  // Therefore: VBAT = VADC_IN1 * 4.9
  // ADC range: 0-4095 = 0-3.3V, so: voltage_mV = (rawAdc / 4095) * 3300 * 4.9
  // Calibrated for actual divider ratio: multiply by 53 instead of 49 (calibration factor 1.0823)
  batteryVoltageMV = (uint16_t)((rawAdc * 3300 * 53) / (4095 * 10));
  
  // DEBUG: log raw ADC and calculated voltage
  static uint32_t lastDebugMs = 0;
  if (millis() - lastDebugMs > 30000) {
    Serial.printf("Battery: raw_adc=%d, voltage=%u mV (%.2f V), charging=%s\n", 
                  rawAdc, batteryVoltageMV, batteryVoltageMV / 1000.0, charging ? "YES" : "NO");
    lastDebugMs = millis();
  }
  
  // Clamp to min/max
  if (batteryVoltageMV < BATTERY_MIN_MV) batteryVoltageMV = BATTERY_MIN_MV;
  if (batteryVoltageMV > BATTERY_MAX_MV) batteryVoltageMV = BATTERY_MAX_MV;
  
  // Calculate battery percentage
  batteryPercent = (uint8_t)((batteryVoltageMV - BATTERY_MIN_MV) * 100 / 
                              (BATTERY_MAX_MV - BATTERY_MIN_MV));
  
  // Track voltage for charging estimation
  uint32_t nowMs = millis();
  if (lastUpdateMs > 0) {
    uint32_t deltaMs = nowMs - lastUpdateMs;
    int16_t deltaVoltageMV = (int16_t)batteryVoltageMV - (int16_t)lastVoltageMV;
    
    // Detect charging: voltage rising > 10mV per update interval
    // No hardware charging detect on V4, infer from voltage trend only
    bool voltageRising = (deltaVoltageMV > 10);
    charging = voltageRising;
    
    // Log charging state changes
    if (charging != lastCharging) {
      Serial.printf("*** CHARGING STATE CHANGED: %s (deltaV=%d mV)\n", 
                    charging ? "CHARGING" : "DISCHARGING", deltaVoltageMV);
      lastCharging = charging;
    }
    
    // Estimate charging time
    if (charging && batteryPercent < 100 && deltaMs > 0 && deltaVoltageMV > 0) {
      // Calculate rate: mV per second
      float ratePerSec = (float)deltaVoltageMV / (deltaMs / 1000.0);
      
      // Remaining voltage needed
      uint16_t remainingMV = BATTERY_MAX_MV - batteryVoltageMV;
      
      if (ratePerSec > 0) {
        uint32_t secondsToFull = (uint32_t)(remainingMV / ratePerSec);
        chargingTimeEstimateMin = secondsToFull / 60;
        
        // Sanity check: estimate should be reasonable (1 min to 24 hours)
        if (chargingTimeEstimateMin < 1) chargingTimeEstimateMin = 1;
        if (chargingTimeEstimateMin > 1440) chargingTimeEstimateMin = 1440;
      }
    } else {
      // Not charging, clear estimate
      chargingTimeEstimateMin = 0;
    }
  }
  lastUpdateMs = nowMs;
  lastVoltageMV = batteryVoltageMV;
}

uint8_t getBatteryPercent() {
  return batteryPercent;
}

bool isCharging() {
  return charging;
}

uint16_t getChargingTimeEstimateMin() {
  return chargingTimeEstimateMin;
}

uint16_t getBatteryVoltageMV() {
  return batteryVoltageMV;
}
