#include <WiFi.h>
#include <freertos/semphr.h>
#include "wifi_manager.h"

// Timing
static const uint32_t SCAN_INTERVAL_MS       = 5000;  // Reduced to 5s since async scan doesn't block
static const uint32_t RECONNECT_BACKOFF_MS   = 4000;

// ================== Status variables ===================
volatile bool wifiConnected = false;          // STA has IP (WL_CONNECTED)
volatile CurrentNet currentNet = CurrentNet::NONE;

String connectedSsid = "";
IPAddress staIP;
IPAddress apIP;

// ================== Umschalt-/Transfer-Steuerung =========================
volatile uint32_t activeUploads = 0;          // Anzahl laufender Uploads/Transfers
volatile bool acceptUploads = true;           // Gate: neue Uploads annehmen?
volatile bool switchRequested = false;        // Umschalten sobald safe

// (Optional) Diagnose: Warum gerade nicht umgeschaltet wird
String switchReason = "";


// ================== internals =================================================
uint32_t lastScanMs = 0;
uint32_t lastConnectAttemptMs = 0;
bool scanInProgress = false;
static SemaphoreHandle_t wifiStateMtx = nullptr;

static void lockWifiState() {
  if (wifiStateMtx) {
    xSemaphoreTake(wifiStateMtx, portMAX_DELAY);
  }
}

static void unlockWifiState() {
  if (wifiStateMtx) {
    xSemaphoreGive(wifiStateMtx);
  }
}

static void updateStatusFromWiFi() {
  wifiConnected = (WiFi.status() == WL_CONNECTED);

  if (!wifiConnected) {
    lockWifiState();
    currentNet = CurrentNet::NONE;
    connectedSsid = "";
    staIP = INADDR_NONE;
    activeUploads = 0; // Sicherheitshalber alle laufenden Uploads als beendet markieren, damit wir bei nächster Gelegenheit switchen können. Akzeptieren von neuen Uploads bleibt wie es ist (z.B. bei switchRequested=false erlauben wir weiter lokale Uploads über AP, auch wenn gerade kein STA verbunden ist).
    acceptUploads = true;
    unlockWifiState();
    return;
  }

  lockWifiState();
  connectedSsid = WiFi.SSID();
  staIP = WiFi.localIP();
  apIP = WiFi.softAPIP();
  if (connectedSsid == SSID_IPHONE) currentNet = CurrentNet::IPHONE;
  else if (connectedSsid == SSID_HOME) currentNet = CurrentNet::HOME;
  else currentNet = CurrentNet::NONE;
  unlockWifiState();
}

bool getConnectedSsidCopy(char* out, size_t outLen) {
  if (!out || outLen == 0) return false;
  if (!wifiStateMtx) {
    out[0] = '\0';
    return false;
  }

  if (xSemaphoreTake(wifiStateMtx, pdMS_TO_TICKS(50)) != pdTRUE) {
    out[0] = '\0';
    return false;
  }

  const char* ssid = connectedSsid.c_str();
  size_t i = 0;
  for (; i + 1 < outLen && ssid[i] != '\0'; i++) {
    out[i] = ssid[i];
  }
  out[i] = '\0';
  xSemaphoreGive(wifiStateMtx);
  return true;
}

static void connectTo(const char* ssid, const char* pass) {
  if (millis() - lastConnectAttemptMs < RECONNECT_BACKOFF_MS) return;
  lastConnectAttemptMs = millis();

  if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == String(ssid)) return;

  // Sauberer Wechsel
  Serial.printf("connectTo: switching to '%s'\n", ssid);
  WiFi.disconnect(true /*wifioff*/, true /*erase*/);
  delay(50);
  WiFi.begin(ssid, pass);
}

static void requestSwitchToIphone() {
  switchRequested = true;
  acceptUploads = false;          // ab jetzt keine neuen Uploads mehr starten
}

static void performSwitchIfSafe() {
  if (!switchRequested) return;

  if (activeUploads > 0) {
    switchReason = "waiting: activeUploads>0";
    return; // warten bis Uploads fertig
  }

  // Safe: jetzt umschalten
  switchReason = "";
  switchRequested = false;
  connectTo(SSID_IPHONE, PASS_IPHONE);

  // Hinweis: acceptUploads bleibt erstmal false, bis wir wieder stabil verbunden sind.
  // Du kannst es bei GOT_IP wieder auf true setzen, siehe Event-Handler unten.
}

static void ensurePriorityConnectionGraceful() {
  // Wenn gerade ein Switch angefordert ist, führen wir den aus sobald safe.
  performSwitchIfSafe();

  // Start async scan if not already in progress
  if (!scanInProgress) {
    WiFi.scanNetworks(true); // true = async mode
    scanInProgress = true;
    return; // Exit and wait for next call
  }

  // Check if async scan is complete
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) {
    return; // Scan still in progress, try again later
  }
  
  if (n == WIFI_SCAN_FAILED) {
    Serial.println("WiFi scan failed, retrying...");
    scanInProgress = false;
    return; // Scan failed, will retry on next interval
  }

  // Scan complete, process results
  scanInProgress = false;
  bool iphoneVisible = false;
  bool homeVisible   = false;

  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (s == SSID_IPHONE) iphoneVisible = true;
    if (s == SSID_HOME)   homeVisible = true;
  }
  WiFi.scanDelete();

  updateStatusFromWiFi();

  // Priorität: iPhone > home
  if (iphoneVisible) {
    // Wenn wir schon auf iPhone sind -> ok
    if (currentNet == CurrentNet::IPHONE) {
      acceptUploads = true; // stabil, Uploads wieder erlauben
      return;
    }

    // Wenn wir auf home sind oder disconnected -> graceful switch anfordern
    requestSwitchToIphone();
    performSwitchIfSafe(); // falls gerade keine Uploads laufen, sofort umschalten
    return;
  }

  // iPhone nicht sichtbar:
  // Wenn wir gerade switchRequested hatten, aber iPhone ist weg -> Switch abbrechen und Uploads wieder erlauben.
  if (switchRequested) {
    switchRequested = false;
    acceptUploads = true;
  }

  // Wenn home sichtbar:
  if (homeVisible) {
    if (currentNet == CurrentNet::HOME) {
      acceptUploads = true;
      return;
    }
    // Wenn wir nicht auf home sind -> verbinden (hier keine besondere Grace nötig,
    // weil wir nur dann hier sind, wenn iPhone nicht sichtbar ist)
    connectTo(SSID_HOME, PASS_HOME);
    return;
  }

  // Nichts sichtbar -> STA trennen (AP bleibt)
  if (WiFi.status() == WL_CONNECTED || WiFi.status() == WL_DISCONNECTED) {
    WiFi.disconnect(false, false);
  }
  updateStatusFromWiFi();
  acceptUploads = true; // lokal über AP darf weiter hochgeladen werden
}

// WiFi Events: bei GOT_IP Uploads wieder erlauben (für STA)
static void onWiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      updateStatusFromWiFi();
      acceptUploads = true;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      updateStatusFromWiFi();
      // acceptUploads bleibt wie es ist; bei SwitchRequest bleibt es false
      break;
    default:
      break;
  }
}

// ================== Upload-Integration (du hängst das in deinen Server) ==================
// Call am Anfang eines Uploads/Transfers.
// return false => bitte Upload ablehnen (z.B. HTTP 503 Busy / "switching network")
bool uploadBegin() {
  if (!acceptUploads) return false;
  activeUploads++;
  return true;
}

// Call am Ende (success oder fail!)
void uploadEnd() {
  if (activeUploads > 0) activeUploads--;
}

void WiFiInit() {
  scanInProgress = false;  // Initialize scan state
  if (!wifiStateMtx) {
    wifiStateMtx = xSemaphoreCreateMutex();
  }
  // uncomment to start an access point on boot
  // WiFi.mode(WIFI_AP_STA);
  // WiFi.setAutoReconnect(false);
  // WiFi.persistent(false);
  WiFi.onEvent(onWiFiEvent);
  // WiFi.setHostname(AP_HOST_NAME);

  // bool apOk = WiFi.softAP(AP_SSID, AP_PASS);
  // Serial.printf("AP '%s' started: %s, AP-IP: %s\n",
  //               AP_SSID, apOk ? "OK" : "FAIL", WiFi.softAPIP().toString().c_str());

  ensurePriorityConnectionGraceful();
  Serial.println("WiFi Task started");
}

void updateWiFi() {
  if (millis() - lastScanMs >= SCAN_INTERVAL_MS) {
    lastScanMs = millis();
    ensurePriorityConnectionGraceful();

    updateStatusFromWiFi();
    // Serial.printf("STA connected=%s, ssid='%s', ip=%s, net=%u, uploads=%lu, acceptUploads=%s, switchReq=%s %s\n",
    //               wifiConnected ? "true" : "false",
    //               connectedSsid.c_str(),
    //               staIP.toString().c_str(),
    //               (unsigned)currentNet,
    //               (unsigned long)activeUploads,
    //               acceptUploads ? "true" : "false",
    //               switchRequested ? "true" : "false",
    //               switchReason.c_str());
  }

  // hier dein Webserver loop / tasks
}
