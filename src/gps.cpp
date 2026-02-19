#include "gps.h"
#include <TimeLib.h>

// UART Pins - used to communicate with the GNSS module
#define GNSS_RX 39
#define GNSS_TX 38
// Power/Control Pins
#define VGNSS_CTRL 34  // active LOW
#define GNSS_WAKE 40   // active HIGH
#define GNSS_RST 42    // active LOW
#define GNSS_PPS 41    // optional, not used in this example

// GPS object
TinyGPSPlus GPS;


// Antenna status
bool antennaOpen = false;
uint32_t lastAntennaMsg = 0;

static void gnssPowerOn() {
  pinMode(VGNSS_CTRL, OUTPUT);
  digitalWrite(VGNSS_CTRL, LOW);
  
  pinMode(GNSS_WAKE, OUTPUT);
  digitalWrite(GNSS_WAKE, HIGH);
  
  pinMode(GNSS_RST, OUTPUT);
  digitalWrite(GNSS_RST, HIGH);
  delay(200);
  
  digitalWrite(GNSS_RST, LOW);
  delay(50);
  digitalWrite(GNSS_RST, HIGH);
  delay(500);
}

void gpsInit() {
  gnssPowerOn();
  Serial1.begin(9600, SERIAL_8N1, GNSS_RX, GNSS_TX);
  Serial.println("GNSS UART started @9600");
}

static int32_t lastSetTimeMs = -800000; // Initialize to a time in the past to set time immediately on first GPS fix
void gpsUpdate() {
  while (Serial1.available()) {
    char c = Serial1.read();
    GPS.encode(c);
    
    static char txtBuf[80];
    static uint8_t idx = 0;
    
    if (c == '$') {
      idx = 0;
    }
    if (idx < sizeof(txtBuf) - 1) {
      txtBuf[idx++] = c;
      txtBuf[idx] = 0;
    }
    
    if (c == '\n') {
      if (strstr(txtBuf, "ANTENNA")) {
        antennaOpen = strstr(txtBuf, "OPEN");
        lastAntennaMsg = millis();
      }
      idx = 0;
      // we may comment this out and rely on the dely in the GPS Task 
      // or move this delay outside while loop to avoid delaying processing of GPS sentences when there is a stream of data coming in
      vTaskDelay(50 / portTICK_PERIOD_MS);  // 5ms yield to allow other tasks to run, especially UI update after GPS fix
    }
    if (GPS.time.isValid()){
      int32_t nowMs = millis();
      if (nowMs - lastSetTimeMs >= 10L * 60L * 1000L && GPS.date.day() !=0) {
        setTime(GPS.time.hour() + (gpsIsWinterTime() ? 1 : 0), GPS.time.minute(), GPS.time.second(), GPS.date.day(), GPS.date.month(), GPS.date.year());
       lastSetTimeMs = nowMs;
      }
    }
  }
}

bool gpsHasTime() {
  return GPS.time.isValid();
}

bool gpsHasLocation() {
  return GPS.location.isValid();
}

bool gpsAntennaStatusValid() {
  return (millis() - lastAntennaMsg) < 5000;
}

// Cache for gpsIsWinterTime() - recalculates only when date changes
static bool gpsIsWinterTime_cachedResult = false;
static uint8_t gpsIsWinterTime_cachedDay = 0;
static uint8_t gpsIsWinterTime_cachedMonth = 0;
static uint16_t gpsIsWinterTime_cachedYear = 0;


bool gpsIsWinterTime() {
  
  uint16_t y = 0;
  uint8_t m = 0;
  uint8_t d = 0;
  uint8_t h = 0;

  if (GPS.date.isValid() && GPS.time.isValid()) {
    y = GPS.date.year();
    m = GPS.date.month();
    d = GPS.date.day();
    h = GPS.time.hour();
  } else {
    y = static_cast<uint16_t>(year());
    m = static_cast<uint8_t>(month());
    d = static_cast<uint8_t>(day());
    h = static_cast<uint8_t>(hour());
    if (y < 1971 || m == 0 || d == 0) {
      return true;
    }
  }

  // Return cached result if date hasn't changed
  if (y == gpsIsWinterTime_cachedYear && m == gpsIsWinterTime_cachedMonth && d == gpsIsWinterTime_cachedDay) {
    return gpsIsWinterTime_cachedResult;
  }

  // Date changed, recalculate
  auto isLeap = [](uint16_t year) {
    return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
  };

  auto daysInMonth = [&](uint16_t year, uint8_t month) {
    static const uint8_t days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month == 2) {
      return static_cast<uint8_t>(isLeap(year) ? 29 : 28);
    }
    return days[month - 1];
  };

  auto calcDayOfWeek = [](uint16_t year, uint8_t month, uint8_t day) {
    static const uint8_t t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    if (month < 3) {
      year -= 1;
    }
    return static_cast<uint8_t>((year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7);
  };

  auto lastSunday = [&](uint16_t year, uint8_t month) {
    uint8_t dim = daysInMonth(year, month);
    for (int day = dim; day >= 1; --day) {
      if (calcDayOfWeek(year, month, static_cast<uint8_t>(day)) == 0) {
        return static_cast<uint8_t>(day);
      }
    }
    return dim;
  };

  bool isSummer = false;
  if (m < 3 || m > 10) {
    isSummer = false;
  } else if (m > 3 && m < 10) {
    isSummer = true;
  } else if (m == 3) {
    uint8_t ls = lastSunday(y, 3);
    if (d > ls) {
      isSummer = true;
    } else if (d < ls) {
      isSummer = false;
    } else {
      isSummer = (h >= 1);
    }
  } else { // October
    uint8_t ls = lastSunday(y, 10);
    if (d < ls) {
      isSummer = true;
    } else if (d > ls) {
      isSummer = false;
    } else {
      isSummer = (h < 1);
    }
  }

  // Update cache
  gpsIsWinterTime_cachedResult = !isSummer;
  gpsIsWinterTime_cachedYear = y;
  gpsIsWinterTime_cachedMonth = m;
  gpsIsWinterTime_cachedDay = d;
  
  return gpsIsWinterTime_cachedResult;
}