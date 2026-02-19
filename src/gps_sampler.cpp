#include "gps_sampler.h"
#include <TimeLib.h>
#include "gps.h"
#include "track_storage.h"
#include "battery.h"

/**
 * Get current timestamp in seconds
 * Uses GPS time if available and valid, otherwise falls back to millis()
 */
uint32_t getTimestampSeconds() {
  time_t t = now();
  if (t > 100000) return (uint32_t) t - (gpsIsWinterTime() ? 1 : 0) * 3600; // epoch seems valid, return UTC time adjusted for winter time if needed
  return (uint32_t)(millis() / 1000);
}


void sampleGPSFix()
{
  FixRec rec = {};  // Initialize to zero (bat=0, flags=0)
  rec.ts = getTimestampSeconds();
  if (rec.ts < 946684800UL) {
    return;
  }
  rec.latE7 = (int32_t)llround(GPS.location.lat() * 1e7);
  rec.lonE7 = (int32_t)llround(GPS.location.lng() * 1e7);
  
  // Fill in battery percentage
  rec.bat = getBatteryPercent();
  
  // Build flags bitfield
  rec.flags = 0;
  if (isCharging()) {
    rec.flags |= FL_CHARGING;
  }
  if (GPS.location.isValid()) {
    rec.flags |= FL_GPS_VALID;
  }
  if (rec.bat <= 15) {
    rec.flags |= FL_LOW_BATTERY;
  }
  // Note: movement state, events, and heartbeat flags are not set here
  // They would be set by a higher-level state machine (future work)
  
  if (trackStorePush(rec))
  {
    Serial.printf("Stored GPS fix: seq=%u, ts=%u, lat=%.6f, lon=%.6f, bat=%u%%, flags=0x%02x\n",
                  rec.seq, rec.ts, rec.latE7 / 1e7, rec.lonE7 / 1e7, rec.bat, rec.flags);
  }
  else
  {
    Serial.println("Failed to store GPS fix due to mutex issue");
  }
}