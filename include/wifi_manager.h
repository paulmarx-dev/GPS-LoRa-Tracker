#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <IPAddress.h>

// ================== Configuration ==================
#include "secrets.h"

// ================== Status variables ===================
enum class CurrentNet : uint8_t { NONE, IPHONE, HOME };

extern volatile bool wifiConnected;
extern volatile CurrentNet currentNet;

extern String connectedSsid;
extern IPAddress staIP;
extern IPAddress apIP;

// ================== Umschalt-/Transfer-Steuerung =========================
extern volatile uint32_t activeUploads;
extern volatile bool acceptUploads;
extern volatile bool switchRequested;

extern String switchReason;

// ================== Function declarations ===================
void WiFiInit();
void updateWiFi();
bool uploadBegin();
void uploadEnd();
bool getConnectedSsidCopy(char* out, size_t outLen);

#endif // WIFI_MANAGER_H
