#ifndef GPS_SAMPLER_H
#define GPS_SAMPLER_H

#include <Arduino.h>
#include <stdint.h>

#define TRACK_CAPACITY 1440 // 1 day @ 1 minute per fix (ring buffer capacity)

/**
 * Get current timestamp in seconds
 * Uses GPS time if available and valid, otherwise falls back to millis()
 * @return Timestamp in seconds (either epoch or ms-based)
 */
uint32_t getTimestampSeconds();

void sampleGPSFix();

#endif // GPS_SAMPLER_H
