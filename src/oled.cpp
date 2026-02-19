#include "oled.h"
#include "wifi_manager.h"
#include "gps.h"
#include "battery.h"
#include "lora_manager.h"
#include "upload_manager.h"
#include "track_storage.h"

// OLED display object - commented out, using Heltec's global from LoRaWan_APP.cpp
// SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
extern SSD1306Wire display;  // Use Heltec's global display object (initialized in LoRaWan_APP.cpp)
// UI object
DisplayUi ui( &display );

// Analog Clock display parameters
int screenW = 128;
int screenH = 64;
int clockCenterX = screenW / 2;
int clockCenterY = screenH / 2; //((screenH - 16) / 2) + 16; // top yellow part is 16 px height
int clockRadius = 23;

// Display On/Off control
void oledVextON()  { pinMode(Vext, OUTPUT); digitalWrite(Vext, LOW); }
void oledVextOFF() { pinMode(Vext, OUTPUT); digitalWrite(Vext, HIGH); }

// utility function for digital clock display: prints leading 0
String twoDigits(int digits) {
    if (digits < 10) { String i = '0' + String(digits); return i; }
    else { return String(digits); }
}


void digitalClockFrame(ScreenDisplay *display, DisplayUiState* state, int16_t x, int16_t y) {
    String timenow = String(hour()) + ":" + twoDigits(minute()) + ":" + twoDigits(second());
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(ArialMT_Plain_24);
    display->drawString(clockCenterX + x , clockCenterY/2 + y, timenow );
}


void analogClockFrame(ScreenDisplay *display, DisplayUiState* state, int16_t x, int16_t y) {
  //  ui.disableIndicator();

  // Draw the clock face
  //  display->drawCircle(clockCenterX + x, clockCenterY + y, clockRadius);
  display->drawCircle(clockCenterX + x, clockCenterY + y, 2);
  //
  //hour ticks
  for ( int z = 0; z < 360; z = z + 30 ) {
    //Begin at 0° and stop at 360°
    float angle = z ;
    angle = ( angle / 57.29577951 ) ; //Convert degrees to radians
    int x2 = ( clockCenterX + ( sin(angle) * clockRadius ) );
    int y2 = ( clockCenterY - ( cos(angle) * clockRadius ) );
    int x3 = ( clockCenterX + ( sin(angle) * ( clockRadius - ( clockRadius / 8 ) ) ) );
    int y3 = ( clockCenterY - ( cos(angle) * ( clockRadius - ( clockRadius / 8 ) ) ) );
    display->drawLine( x2 + x , y2 + y , x3 + x , y3 + y);
  }

  // display second hand
  float angle = second() * 6 ;
  angle = ( angle / 57.29577951 ) ; //Convert degrees to radians
  int x3 = ( clockCenterX + ( sin(angle) * ( clockRadius - ( clockRadius / 5 ) ) ) );
  int y3 = ( clockCenterY - ( cos(angle) * ( clockRadius - ( clockRadius / 5 ) ) ) );
  display->drawLine( clockCenterX + x , clockCenterY + y , x3 + x , y3 + y);
  //
  // display minute hand
  angle = minute() * 6 ;
  angle = ( angle / 57.29577951 ) ; //Convert degrees to radians
  x3 = ( clockCenterX + ( sin(angle) * ( clockRadius - ( clockRadius / 4 ) ) ) );
  y3 = ( clockCenterY - ( cos(angle) * ( clockRadius - ( clockRadius / 4 ) ) ) );
  display->drawLine( clockCenterX + x , clockCenterY + y , x3 + x , y3 + y);
  //
  // display hour hand
  angle = hour() * 30 + int( ( minute() / 12 ) * 6 )   ;
  angle = ( angle / 57.29577951 ) ; //Convert degrees to radians
  x3 = ( clockCenterX + ( sin(angle) * ( clockRadius - ( clockRadius / 2 ) ) ) );
  y3 = ( clockCenterY - ( cos(angle) * ( clockRadius - ( clockRadius / 2 ) ) ) );
  display->drawLine( clockCenterX + x , clockCenterY + y , x3 + x , y3 + y);
}


static uint32_t lastGPSUi = 0;
void GPSFrame(ScreenDisplay *display, DisplayUiState* state, int16_t x, int16_t y) {

    bool hasTime = GPS.time.isValid();
    bool hasLoc = GPS.location.isValid();
    bool antennaStatusValid = gpsAntennaStatusValid();
  
    char t[20], la[24], lo[24];

    if (hasTime) {
        snprintf(t, sizeof(t), "%02d:%02d:%02d.%02d",
                GPS.time.hour() + (gpsIsWinterTime() ? 1 : 0), GPS.time.minute(), GPS.time.second(), GPS.time.centisecond());
    } else {
        snprintf(t, sizeof(t), "--:--:--.--");
    }

    if (hasLoc) {
        snprintf(la, sizeof(la), "LAT: %.6f", GPS.location.lat());
        snprintf(lo, sizeof(lo), "LON: %.6f", GPS.location.lng());
    } else {
        snprintf(la, sizeof(la), "LAT: ----");
        snprintf(lo, sizeof(lo), "LON: ----");
    }

    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(ArialMT_Plain_10);
    display->drawString(0 + x, 0*LH + y, t);
    display->drawString(0 + x, 1*LH + y, la);
    display->drawString(0 + x, 2*LH + y, lo);

    if (!hasTime || !hasLoc) {
        int progress = (millis() / 100) % 100;
        display->drawProgressBar(0 + x, 42 + y, 120, 5, progress);
    }


    display->setTextAlignment(TEXT_ALIGN_RIGHT);
    if (antennaStatusValid) {
        display->drawString(117 + x, 0 + y, antennaOpen ? "ANT OPEN" : "ANT OK");
    }


    // Write to serial for debugging - throttled to once per 10 seconds only
    if (millis() - lastGPSUi >= 10000) {
        // Serial.println(t);
        // Serial.println(la);
        // Serial.println(lo);
        lastGPSUi = millis();
    }

}


void WLANFrame(ScreenDisplay *display, DisplayUiState* state, int16_t x, int16_t y) {
  char stSsid[32] = "";
  char stIP[20] = "";
  const char* netName = "None";

  if (currentNet == CurrentNet::IPHONE) netName = "iPhone";
  else if (currentNet == CurrentNet::HOME) netName = "Home";


  if (WiFi.status() == WL_CONNECTED) {
    snprintf(stSsid, sizeof(stSsid), "%s %d dBm", netName, WiFi.RSSI());
    snprintf(stIP, sizeof(stIP), "%s", staIP.toString().c_str());
  } else {
    snprintf(stSsid, sizeof(stSsid), "Not connected");
    snprintf(stIP, sizeof(stIP), "---.---.---.---");
  }
  
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(ArialMT_Plain_10);
    display->drawString(0*LH + x, 0*LH + y, "WiFi NETWORKS");
    // display->drawString(0*LH + x, 1*LH + y, AP_HOST_NAME);
    display->drawString(0*LH + x, 2*LH + y, stSsid);
    display->drawString(0*LH + x, 3*LH + y, stIP);
}


void transmissionStatsFrame(ScreenDisplay *display, DisplayUiState* state, int16_t x, int16_t y) {
  // Get stats
  size_t totalPoints = trackStoreSize();
  uint32_t ackedTs = trackStoreGetAckedTs();
  size_t ackedCount = 0;
  
  uint32_t lastWiFiMs = getLastWiFiTxMs();
  uint32_t wiFiCount = getWiFiTxCount();
  uint32_t lastLoraMs = getLastLoraTxMs();
  uint32_t loraCount = getLoraTxCount();
  
  uint32_t pendingCount = totalPoints - wiFiCount;
  
  // Format timestamps as HH:MM:SS
  char wiFiTime[12] = "--:--:--";
  char loraTime[12] = "--:--:--";
  
  if (lastWiFiMs > 0) {
    uint32_t elapsedSec = (millis() - lastWiFiMs) / 1000;
    uint32_t hours = elapsedSec / 3600;
    uint32_t minutes = (elapsedSec % 3600) / 60;
    uint32_t seconds = elapsedSec % 60;
    // Clamp to 23:59:59 for display
    if (hours > 23) hours = 23;
    snprintf(wiFiTime, sizeof(wiFiTime), "%02lu:%02lu:%02lu", hours, minutes, seconds);
  }
  
  if (lastLoraMs > 0) {
    uint32_t elapsedSec = (millis() - lastLoraMs) / 1000;
    uint32_t hours = elapsedSec / 3600;
    uint32_t minutes = (elapsedSec % 3600) / 60;
    uint32_t seconds = elapsedSec % 60;
    if (hours > 23) hours = 23;
    snprintf(loraTime, sizeof(loraTime), "%02lu:%02lu:%02lu", hours, minutes, seconds);
  }
  
  char wiFiStatus[8] = "off";
  char loraStatus[8] = "sleep";
  
  if (wifiConnected) {
    snprintf(wiFiStatus, sizeof(wiFiStatus), "on");
  }
  
  extern bool hasJoined_public;  // You may need to expose this differently
  extern bool hasJoined;         // Try direct access to lora_manager's hasJoined
  
  // Draw frame
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(0*LH + x, 0*LH + y, "DATA & TRANSMISSIONS");
  
  char line1[32];
  snprintf(line1, sizeof(line1), "%lu | %lu [a] | %lu [p]", (unsigned long)totalPoints, (unsigned long)wiFiCount, (unsigned long)pendingCount);
  display->drawString(0 + x, 1*LH + y, line1);
  
  char line2[64];
  snprintf(line2, sizeof(line2), "WiFi: %lu @ %s [%s]", (unsigned long)wiFiCount, wiFiTime, wiFiStatus);
  display->drawString(0 + x, 2*LH + y, line2);
  
  char line3[64];
  snprintf(line3, sizeof(line3), "LoRa: %lu @ %s [%s]", (unsigned long)loraCount, loraTime, loraStatus);
  display->drawString(0 + x, 3*LH + y, line3);
  
  // Show transmission status on line 4
  char txState[32] = "";
  if (isWiFiTxActive()) {
    snprintf(txState, sizeof(txState), "[WiFi...]");
  } else if (isLoraTxActive()) {
    snprintf(txState, sizeof(txState), "[LoRa...]");
  } else {
    snprintf(txState, sizeof(txState), "[idle]");
  }

  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->drawString(117, 54, transitionMode);
  display->drawString(100 + x, 53 + y, txState);
}


void BatteryFrame(ScreenDisplay *display, DisplayUiState* state, int16_t x, int16_t y) {
  uint8_t percent = getBatteryPercent();
  uint16_t voltageMV = getBatteryVoltageMV();
  bool charging = isCharging();
  uint16_t chargeTimeMin = getChargingTimeEstimateMin();
  
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(0 + x, 0 + y, "BATTERY");
  
  // Battery percentage (large, left side)
  display->setFont(ArialMT_Plain_24);
  display->drawString(0 + x, 1*LH + y, String(percent) + "%");
  
  // Voltage and charging on right side
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  char voltStr[16];
  snprintf(voltStr, sizeof(voltStr), "%.2f V", voltageMV / 1000.0);
  display->drawString(117 + x, 1*LH + y, voltStr);
  
  if (charging) {
    if (chargeTimeMin > 0 && chargeTimeMin < 1440) {
      char chargeStr[20];
      if (chargeTimeMin >= 60) {
        snprintf(chargeStr, sizeof(chargeStr), "+%u:%02u", chargeTimeMin / 60, chargeTimeMin % 60);
      } else {
        snprintf(chargeStr, sizeof(chargeStr), "+%u m", chargeTimeMin);
      }
      display->drawString(117 + x, 2*LH + y, chargeStr);
    } else {
      display->drawString(117 + x, 2*LH + y, "CHARGING");
    }
  }
  
  // Low battery warning (if applicable)
  if (percent < 10) {
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(ArialMT_Plain_16);
    display->drawString(64 + x, 3*LH + y, "LOW!");
  }
  
  // Draw progress bar at bottom
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawProgressBar(0 + x, 44 + y, 120, 5, percent);
}


// USAGE
// char timeStr[20];
// millisToTime(millis(), timeStr, sizeof(timeStr));
// Serial.println(timeStr);  // Prints: "00:02:15" or "1:02:30:15"
String millisToTime() {
    char timeStr[20];  
      
    uint32_t totalSeconds = millis() / 1000;

    uint32_t seconds = totalSeconds % 60;
    uint32_t minutes = (totalSeconds / 60) % 60;
    uint32_t hours = (totalSeconds / 3600) % 24;
    uint32_t days = totalSeconds / 86400;

    if (days > 0) {
    snprintf(timeStr, sizeof(timeStr), "%lu:%02lu:%02lu:%02lu", 
                days, hours, minutes, seconds);
    } else {
    snprintf(timeStr, sizeof(timeStr), "%02lu:%02lu:%02lu", 
                hours, minutes, seconds);
    }
    
    return String(timeStr);
}

void msOverlay(ScreenDisplay *display, DisplayUiState* state) {
    // char up[12];
    // snprintf(up, sizeof(up), "%lus", (unsigned long)(millis() / 1000));
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(ArialMT_Plain_10);
    display->drawString(0, 54, millisToTime());

    display->setTextAlignment(TEXT_ALIGN_RIGHT);
    display->drawString(117, 54, transitionMode);
}


// This array keeps function pointers to all frames
// frames are the single views that slide in
FrameCallback frames[] = { GPSFrame, WLANFrame, transmissionStatsFrame, BatteryFrame, analogClockFrame, digitalClockFrame };
int frameCount = sizeof(frames) / sizeof(frames[0]);        // how many frames are there?

// Overlays are statically drawn on top of a frame eg. a clock
OverlayCallback overlays[] = { msOverlay };
int overlaysCount = sizeof(overlays) / sizeof(overlays[0]);  // how many overlays are there?




void uiInit() {
    oledVextON();
    delay(100);

    ui.disableAutoTransition(); // Disable auto-transition for manual control
    ui.setTimePerTransition(200); // Set transition time to 200ms

    ui.setTargetFPS(30);
    ui.setIndicatorPosition(RIGHT);           // TOP, LEFT, BOTTOM, RIGHT
    ui.setIndicatorDirection(LEFT_RIGHT);     // Defines where the first frame is located in the bar.
    ui.setFrameAnimation(SLIDE_UP);         // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_UP, SLIDE_DOWN
    
    // Customize the active and inactive symbol
    ui.setActiveSymbol(activeSymbol);
    ui.setInactiveSymbol(inactiveSymbol);

    ui.setFrames(frames, frameCount);         // Add frames
    ui.setOverlays(overlays, overlaysCount);  // Add overlays

    //ui.disableAutoTransition();
    ui.init();                                // Initialize the UI. This will init the display too.
}   



// --------------------------------------------------------------------------------
// OLED display functions - without UI framework, for direct control of the display
// --------------------------------------------------------------------------------

void oledInit() {
  oledVextON();
  delay(100);
  display.init();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.clear();
  display.drawString(0, 0, "Initializing ...");
  display.display();
}

void oledDisplaySearching(int progress) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 10, "Searching GPS ...");
  display.drawProgressBar(0, 42, 120, 5, progress);
  display.display();
}

void oledDisplayUpdate(bool hasTime, bool hasLoc, 
                       uint8_t hour, uint8_t minute, uint8_t second, uint8_t centisecond,
                       double latitude, double longitude,
                       bool antennaOpen, bool antennaStatusValid) {
  display.clear();
  
  char t[20], la[24], lo[24];
  
  if (hasTime) {
    snprintf(t, sizeof(t), "%02d:%02d:%02d.%02d",
             hour, minute, second, centisecond);
  } else {
    snprintf(t, sizeof(t), "--:--:--.--");
  }
  
  if (hasLoc) {
    snprintf(la, sizeof(la), "LAT: %.6f", latitude);
    snprintf(lo, sizeof(lo), "LON: %.6f", longitude);
  } else {
    snprintf(la, sizeof(la), "LAT: ----");
    snprintf(lo, sizeof(lo), "LON: ----");
  }
  
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, t);
  display.drawString(0, 12, la);
  display.drawString(0, 24, lo);

if (!hasTime || !hasLoc) {
    int progress = (millis() / 100) % 100;
    display.drawProgressBar(0, 42, 120, 4, progress);
}

  
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  if (antennaStatusValid) {
    display.drawString(117, 0, antennaOpen ? "ANT OPEN" : "ANT OK");
  }
  
  char up[12];
  snprintf(up, sizeof(up), "%lus", (unsigned long)(millis() / 1000));
  display.drawString(117, 54, up);
  
  display.display();
}
