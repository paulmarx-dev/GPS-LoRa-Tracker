#include "lora_manager.h"
#include <RadioLib.h>
#include "Arduino.h"
#include "secrets.h"
#include "track_storage.h"
#include "wifi_manager.h"
#include "gps.h"
#include <math.h>

// ============= HELTEC V4 PINOUT =============
#define RADIO_SCLK_PIN 9
#define RADIO_MISO_PIN 11
#define RADIO_MOSI_PIN 10
#define RADIO_CS_PIN 8
#define RADIO_RST_PIN 12
#define RADIO_DIO1_PIN 14
#define RADIO_BUSY_PIN 13

// Heltec V4 FEM pins
#define PIN_VFEM 7    // VFEM_Ctrl
#define PIN_FEM_EN 2  // FEM_EN / CSD (keep HIGH always)
#define PIN_TX_EN 46  // PA_TX_EN (HIGH only in TX)

// ================= MOVEMENT CONFIG =================
static constexpr float MOVE_START_KMH = 2.0f;   
static constexpr float MOVE_STOP_KMH  = 1.0f;   
static constexpr float DIST_TRIGGER_M = 50.0f;  // 50m

static constexpr uint32_t HEARTBEAT_INTERVAL_MS = 15 * 60 * 1000;  // 15 minutes
static constexpr uint32_t MIN_SEND_INTERVAL_MS  = 2.5 * 60 * 1000;   // 2.5 minutes


// ============= RADIOLIB INSTANCES =============
SX1262 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
LoRaWANNode* node = NULL;

// ============= STATE VARIABLES =============
static bool isInitialized = false;
static bool hasJoined = false;
static unsigned long lastTxMs = 0;
static const unsigned long TX_INTERVAL_MS = 60000;  // 60 second interval

static uint32_t lastSendMs = 0;
static uint32_t lastHeartbeatMs = 0;
static int32_t lastLatE7 = 0;
static int32_t lastLonE7 = 0;
static bool moving = false;
static bool prevMoving = false;

// ============= TX STATS TRACKING =============
static uint32_t lastLoraTxMs = 0;
static uint32_t loraTxCount = 0;
static volatile bool loraTxActive = false;

// ============= MAC COMMAND CIDs =============
static constexpr uint8_t CID_LINKCHECK_REQ = 0x02;
static constexpr uint8_t CID_DEVICETIME_REQ = 0x0D;

// ============= HELPER FUNCTIONS =============

static void printHex(const uint8_t* b, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (b[i] < 0x10) Serial.print('0');
    Serial.print(b[i], HEX);
    if (i + 1 < n) Serial.print(' ');
  }
}

static uint64_t euiToU64(const uint8_t eui[8]) {
  uint64_t v = 0;
  for (int i = 0; i < 8; i++) {
    v = (v << 8) | (uint64_t)eui[i];  // MSB-first
  }
  return v;
}

static void initFEM() {
  pinMode(PIN_VFEM, OUTPUT);
  pinMode(PIN_FEM_EN, OUTPUT);
  pinMode(PIN_TX_EN, OUTPUT);

  digitalWrite(PIN_VFEM, HIGH);
  digitalWrite(PIN_FEM_EN, HIGH);
  digitalWrite(PIN_TX_EN, LOW);
}

static void printEvent(const char* tag, const LoRaWANEvent_t& ev) {
  Serial.print("[");
  Serial.print(tag);
  Serial.println("]");
  Serial.print("  dir=");
  Serial.print(ev.dir);
  Serial.print(" confirmed=");
  Serial.print(ev.confirmed);

  Serial.print("  DR=");
  Serial.print(ev.datarate);
  Serial.print(" freq(MHz)=");
  Serial.print(ev.freq, 3);
  Serial.print(" power_or_rssi=");
  Serial.print(ev.power);
  Serial.print(" FCnt=");
  Serial.print(ev.fCnt);
  Serial.print(" FPort=");
  Serial.println(ev.fPort);
}

// ============= INITIALIZATION =============

void loraInit() {
  delay(100);
  Serial.flush();

  Serial.println("\n========================================");
  Serial.println("[LoRaWAN] Initializing with working V4 sketch pattern");
  Serial.println("========================================");

  // Initialize FEM
  Serial.println("[V4] Initializing FEM (frontend module)...");
  initFEM();

  // Initialize SPI
  Serial.println("[SX1262] Configuring SPI...");
  SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN, RADIO_CS_PIN);
  SPI.setFrequency(1000000);  // 1 MHz safe for SX1262
  Serial.println("[SX1262] SPI configured");

  // Set external RF switch before radio.begin()
  Serial.print("[V4] Setting RF switch pins... ");
  radio.setRfSwitchPins(RADIOLIB_NC, PIN_TX_EN);  // TX_EN on GPIO46
  Serial.println("OK");

  // Initialize radio
  Serial.print("[SX1262] Initializing radio... ");
  int16_t state = radio.begin(868.1,    // freq
                             125.0,     // bw
                             9,         // sf
                             7,         // cr
                             0x34,      // sync word
                             22,        // power
                             8,         // preamble
                             1.8,       // tcxoVoltage (critical for V4!)
                             false);    // useRegulatorLDO

  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("FAILED (");
    Serial.print(state);
    Serial.println(")");
    Serial.flush();
    return;
  }
  Serial.println("SUCCESS");

  // Internal DIO2 RF switch
  Serial.print("[SX1262] Setting DIO2 as RF switch... ");
  int16_t sw = radio.setDio2AsRfSwitch(true);
  Serial.print("state=");
  Serial.println(sw);

  // Create node instance
  Serial.print("[LoRaWAN] Creating node instance... ");
  if (node != NULL) {
    delete node;
  }
  node = new LoRaWANNode(&radio, &EU868, 0);  // subBand=0 for EU868
  if (!node) {
    Serial.println("FAILED - Memory allocation error");
    Serial.flush();
    return;
  }
  Serial.println("OK");

  // Load credentials from secrets.h
  uint8_t devEui_arr[] = LORAWAN_DEVEUI;
  uint8_t joinEui_arr[] = LORAWAN_JOINEUI;
  uint8_t appKey_arr[] = LORAWAN_APPKEY;
  uint8_t nwkKey_arr[] = LORAWAN_NWKKEY;

  uint64_t joinEui = euiToU64(joinEui_arr);
  uint64_t devEui = euiToU64(devEui_arr);

  // Configure OTAA
  Serial.println("[LoRaWAN] Configuring OTAA...");
  node->beginOTAA(joinEui, devEui, nwkKey_arr, appKey_arr);
  Serial.println("  beginOTAA: OK");

  // Disable ADR for predictable behavior
  Serial.println("[LoRaWAN] Disabling ADR...");
  node->setADR(false);

  // Initial join attempt
  Serial.println("[LoRaWAN] Attempting initial join...");
  state = node->activateOTAA();
  Serial.print("  activateOTAA: ");
  Serial.println(state);

  if (state == RADIOLIB_ERR_NONE || state == RADIOLIB_LORAWAN_NEW_SESSION) {
    Serial.println("[LoRaWAN] Initial join SUCCEEDED");
    hasJoined = true;
  } else {
    Serial.println("[LoRaWAN] Initial join attempt failed (will retry in loop)");
    hasJoined = false;
  }

  isInitialized = true;
  lastTxMs = millis();

  Serial.println("[LoRaWAN] ========== INIT COMPLETE ==========");
  Serial.flush();
}


// ============= PERIODIC UPDATE =============

void loraUpdate() {
  if (wifiConnected) { return; }  // Only send via LoRa if WiFi is unavailable
  if (!isInitialized || !node) { return; }

  // If not joined, retry join every 10 seconds
  if (!hasJoined) {
    static unsigned long lastJoinRetry = 0;
    if (millis() - lastJoinRetry >= 10000) {
      Serial.println("\n[JOIN] Retrying activateOTAA...");
      int16_t state = node->activateOTAA();
      Serial.print("[JOIN] activateOTAA: ");
      Serial.println(state);

      if (state == RADIOLIB_ERR_NONE || state == RADIOLIB_LORAWAN_NEW_SESSION) {
        Serial.println("[JOIN] SUCCESS - Device joined!");
        hasJoined = true;
      }
      lastJoinRetry = millis();
    }
    return;  // Don't try to transmit until joined
  }

  checkAndSend();
}

// ============= POWER MANAGEMENT =============

void loraStop() {
  if (!node || !isInitialized) { return; }
  
  // Put radio into sleep mode (minimal current draw ~<1µA)
  radio.sleep();
  hasJoined = false;  // Will need to rejoin when restarted
  
  Serial.println("[LoRaWAN] Radio sleeping (WiFi connected) - Power save mode");
  Serial.flush();
}

void loraResume() {
  if (!node || !isInitialized) { return; }
  
  // Reset join state - radio will wake from sleep automatically on next transmission
  hasJoined = false;  // Will rejoin on next loraUpdate
  lastTxMs = millis();
  
  Serial.println("[LoRaWAN] Radio ready to resume (WiFi disconnected)");
  Serial.flush();
}

// ================= DISTANCE CALCULATION =================
static float distanceMeters(int32_t lat1E7, int32_t lon1E7,
                            int32_t lat2E7, int32_t lon2E7)
{
    const float R = 6371000.0f;

    float lat1 = lat1E7 / 1e7f * DEG_TO_RAD;
    float lat2 = lat2E7 / 1e7f * DEG_TO_RAD;
    float dLat = lat2 - lat1;
    float dLon = (lon2E7 - lon1E7) / 1e7f * DEG_TO_RAD;

    float a = sin(dLat/2)*sin(dLat/2) +
              cos(lat1)*cos(lat2) *
              sin(dLon/2)*sin(dLon/2);

    return 2 * R * atan2(sqrt(a), sqrt(1-a));
}

// send when 
// - outside min send interval (rate limiting)
// - heartbeat due
// - latest GPS fix is present and valid
// - movement changed: began moving, stopped, moved > DIST_TRIGGER_M
void checkAndSend() {
  // don't send more often than MIN_SEND_INTERVAL_S
  if (millis() - lastSendMs < MIN_SEND_INTERVAL_MS) { return; }
  if (!hasJoined || !node) { return; } // [TX] Not joined yet, skipping transmit

  uint32_t nowMs = millis();
  const bool heartBeatDue = (nowMs - lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS);
  const bool movementChanged = (moving != prevMoving);
  bool reasonDistance = false; 
  
  // no valid fix - don't send
  FixRec latestFix;
  if (!trackStoreGetLatest(latestFix)) { return; }
  if (latestFix.ts == 0 || latestFix.ts < 946684800UL) { return; }
  if (latestFix.latE7 < -900000000 || latestFix.latE7 > 900000000) { return; }
  if (latestFix.lonE7 < -1800000000 || latestFix.lonE7 > 1800000000) { return; }

  int32_t latE7 = latestFix.latE7;
  int32_t lonE7 = latestFix.lonE7;
  float speedKmh = GPS.speed.isValid() ? GPS.speed.kmph() : 0.0f;
  
  // Movement detection
  if (!moving && speedKmh >= MOVE_START_KMH) {
    moving = true;
  } else if (moving && speedKmh <= MOVE_STOP_KMH) {
    moving = false;
  }
  
  if (moving && lastLatE7 != 0) {
    float dist = distanceMeters(lastLatE7, lastLonE7, latE7, lonE7);
    if (dist >= DIST_TRIGGER_M) { reasonDistance = true; }
  }
  
  bool shouldSend = movementChanged || reasonDistance || heartBeatDue;

  if (shouldSend) {
    sendPayload(latestFix.ts, latE7, lonE7, latestFix.bat);
    lastSendMs = millis();
    lastLatE7 = latE7;
    lastLonE7 = lonE7;
    if (movementChanged) { prevMoving = moving; }
    if (heartBeatDue) { lastHeartbeatMs = nowMs;}
  }
  
}



// ============= TRANSMISSION =============


// Build 13-byte GPS payload
// Byte 0-3:   uint32_t timestamp (big-endian)
// Byte 4-7:   int32_t lat*1e7 (big-endian)
// Byte 8-11:  int32_t lon*1e7 (big-endian)
// Byte 12:    uint8_t battery %
void sendPayload(int32_t ts, int32_t latE7, int32_t lonE7, uint8_t bat) {

  // Track transmission
  lastLoraTxMs = millis();
  loraTxCount++;

  uint32_t uts = static_cast<uint32_t>(ts);
  uint32_t ulat = static_cast<uint32_t>(latE7);
  uint32_t ulon = static_cast<uint32_t>(lonE7);

  uint8_t payload[13];
  payload[0] = (uint8_t)((uts >> 24) & 0xFF);
  payload[1] = (uint8_t)((uts >> 16) & 0xFF);
  payload[2] = (uint8_t)((uts >> 8) & 0xFF);
  payload[3] = (uint8_t)(uts & 0xFF);
  
  payload[4] = (uint8_t)((ulat >> 24) & 0xFF);
  payload[5] = (uint8_t)((ulat >> 16) & 0xFF);
  payload[6] = (uint8_t)((ulat >> 8) & 0xFF);
  payload[7] = (uint8_t)(ulat & 0xFF);
  
  payload[8] = (uint8_t)((ulon >> 24) & 0xFF);
  payload[9] = (uint8_t)((ulon >> 16) & 0xFF);
  payload[10] = (uint8_t)((ulon >> 8) & 0xFF);
  payload[11] = (uint8_t)(ulon & 0xFF);
  
  payload[12] = bat;

  // Prepare RX buffer for potential downlinks
  uint8_t downlink[255];
  size_t downlinkLen = sizeof(downlink);

  // Event tracking
  LoRaWANEvent_t evUp{};
  LoRaWANEvent_t evDown{};

  Serial.println("\n========================================");
  Serial.print("[TX] GPS Fix seq=");
  //Serial.print(fix.seq);
  Serial.print(" lat=");
  Serial.print(latE7 / 1e7, 6);
  Serial.print(" lon=");
  Serial.print(lonE7 / 1e7, 6);
  Serial.print(" bat=");
  Serial.print(bat);
  Serial.println("%");
  Serial.print("     Payload: ");
  printHex(payload, sizeof(payload));
  Serial.println();

  // Mark transmission active
  loraTxActive = true;

  // Transmit with RX window
  int16_t txState = node->sendReceive(
    payload, sizeof(payload),
    1,                      // FPort=1
    downlink, &downlinkLen,
    false,                  // confirmed=false
    &evUp, &evDown
  );

  // Mark transmission complete
  loraTxActive = false;

  Serial.print("[TX] sendReceive result: ");
  Serial.println(txState);
  Serial.print("     Last ToA(ms): ");
  Serial.println((uint32_t)node->getLastToA());

  printEvent("UP", evUp);

  if (txState > 0) {
    // Received downlink
    Serial.println("[RX] DOWNLINK RECEIVED");
    printEvent("DOWN", evDown);
    Serial.print("     downLen=");
    Serial.println(downlinkLen);
    Serial.print("     data: ");
    printHex(downlink, downlinkLen);
    Serial.println();

    // Parse MAC answers if present
    uint8_t margin = 0, gwCnt = 0;
    if (node->getMacLinkCheckAns(&margin, &gwCnt) == RADIOLIB_ERR_NONE) {
      Serial.print("     LinkCheckAns margin(dB)=");
      Serial.print(margin);
      Serial.print(" gwCnt=");
      Serial.println(gwCnt);
    }

    uint32_t ts = 0;
    uint8_t ms = 0;
    if (node->getMacDeviceTimeAns(&ts, &ms, true) == RADIOLIB_ERR_NONE) {
      Serial.print("     DeviceTimeAns unix=");
      Serial.print(ts);
      Serial.print(".");
      Serial.println(ms);
    }
  } else if (txState == 0) {
    Serial.println("[RX] No downlink (normal for unconfirmed)");
  } else {
    Serial.print("[ERROR] sendReceive failed with code ");
    Serial.println(txState);
  }

  Serial.println("========================================");
  Serial.flush();
}

// ============= TX STATS GETTERS =============

uint32_t getLastLoraTxMs() {
  return lastLoraTxMs;
}

uint32_t getLoraTxCount() {
  return loraTxCount;
}

bool isLoraTxActive() {
  return loraTxActive;
}



