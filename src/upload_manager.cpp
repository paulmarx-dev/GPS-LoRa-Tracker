#include "upload_manager.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "wifi_manager.h"
#include "battery.h"

// ============= TX STATS TRACKING =============
static uint32_t lastWiFiTxMs = 0;
static uint32_t wiFiTxCount = 0;
static volatile bool wiFiTxActive = false;

String buildJSONBatch(FixRec* recs, size_t n) {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();

  char ssidBuf[33] = "";
  getConnectedSsidCopy(ssidBuf, sizeof(ssidBuf));

  for (size_t i = 0; i < n; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["seq"]   = recs[i].seq;
    o["ts"]    = recs[i].ts;
    o["latE7"] = recs[i].latE7;
    o["lonE7"] = recs[i].lonE7;
    o["net"]   = ssidBuf;
    o["ch"]    = "wifi";
    o["bat"]   = recs[i].bat;       // Battery from stored record
    o["flags"] = recs[i].flags;     // Flags from stored record
  }

  String out;
  serializeJson(doc, out);
  return out;
}

bool parseACKResponse(const String& response, uint32_t& ackedTs) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, response);

  if (error) {
    Serial.print("parseACKResponse: JSON parse error: ");
    Serial.println(error.c_str());
    return false;
  }

  if (!doc["ackedTs"].is<uint32_t>()) {
    Serial.println("parseACKResponse: 'ackedTs' key not found or invalid type");
    return false;
  }

  ackedTs = doc["ackedTs"].as<uint32_t>();
  Serial.printf("parseACKResponse: Successfully parsed ackedTs = %u\n", ackedTs);
  return true;
}

void uploadBatchOverWiFi(FixRec batch[MAX_UPLOAD_BATCH_SIZE]) {
  if (!wifiConnected) return;
  if (!uploadBegin()) return;

  uint32_t ackedTs = trackStoreGetAckedTs();
  size_t n = trackStoreGetBatch(batch, MAX_UPLOAD_BATCH_SIZE, ackedTs);
  if (n == 0) { uploadEnd(); return; }

  String jsonPayload = buildJSONBatch(batch, n);
  Serial.printf("Uploading %u GPS fixes (payload bytes=%u)\n", (unsigned)n, (unsigned)jsonPayload.length());

  HTTPClient http;
  http.setTimeout(10000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  WiFiClientSecure client;
  client.setInsecure(); // quick test; later setCACert(...)

  if (!http.begin(client, UPLOAD_URL)) {
    Serial.println("http.begin failed");
    uploadEnd();
    return;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-Token", HTTP_X_API_TOKEN);
  http.addHeader("X-Device-Id", HTTP_X_DEVICE_ID);

  // Mark transmission active
  wiFiTxActive = true;

  int code = http.POST(jsonPayload);
  String response = http.getString(); // read once

  // Mark transmission complete
  wiFiTxActive = false;

  Serial.printf("Upload response code: %d, body: %s\n", code, response.c_str());

  if (code == 200) {
    uint32_t newAckedTs = ackedTs;
    if (parseACKResponse(response, newAckedTs) && newAckedTs > ackedTs) {
      trackStoreSetAckedTs(newAckedTs);
      Serial.printf("Updated ackedTs to %u\n", newAckedTs);
      
      // Track successful upload
      lastWiFiTxMs = millis();
      wiFiTxCount += n;
    }
  } else if (code > 0) {
    Serial.printf("Upload failed with HTTP code %d\n", code);
  } else {
    Serial.printf("HTTP POST failed, error: %s\n", http.errorToString(code).c_str());
  }

  http.end();
  uploadEnd();
}

// ============= TX STATS GETTERS =============

uint32_t getLastWiFiTxMs() {
  return lastWiFiTxMs;
}

uint32_t getWiFiTxCount() {
  return wiFiTxCount;
}

bool isWiFiTxActive() {
  return wiFiTxActive;
}
