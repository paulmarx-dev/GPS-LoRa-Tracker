#ifndef UPLOAD_MANAGER_H
#define UPLOAD_MANAGER_H

#include <Arduino.h>
#include "track_storage.h"
#include "secrets.h"

#define UPLOAD_INTERVAL_MS 60000 // 60 seconds
#define MAX_UPLOAD_BATCH_SIZE 60

/**
 * Build a JSON array string from GPS fix records
 * @param recs Array of FixRec records
 * @param n Number of records
 * @return JSON string representation of the batch
 */
String buildJSONBatch(FixRec* recs, size_t n);

/**
 * Parse ACK response using ArduinoJson
 * Expected format: {"ackedTs": 123}
 * @param response Server response string
 * @param ackedTs Output parameter for acknowledged timestamp
 * @return true if parsing succeeded, false otherwise
 */
bool parseACKResponse(const String& response, uint32_t& ackedTs);

/**
 * Upload a batch of GPS fixes to the server via WiFi
 * Retrieves unacked records from track storage, sends to server, updates acked timestamp
 * @param batch Pre-allocated buffer for holding batch records
 * @return true on successful upload attempt, false if upload skipped or failed
 */
void uploadBatchOverWiFi(FixRec batch[MAX_UPLOAD_BATCH_SIZE]);

/**
 * Get last WiFi upload timestamp (milliseconds)
 */
uint32_t getLastWiFiTxMs();

/**
 * Get total points uploaded via WiFi
 */
uint32_t getWiFiTxCount();

/**
 * Check if WiFi transmission is currently active
 */
bool isWiFiTxActive();

#endif // UPLOAD_MANAGER_H
