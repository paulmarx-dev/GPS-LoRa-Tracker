#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "gps.h"
#include "oled.h"
#include "button.h"  
#include "wifi_manager.h"
#include "track_storage.h"
#include "gps_sampler.h"
#include "upload_manager.h"
#include "battery.h"
#include "lora_manager.h"

#define GPS_SAMPLING_RATE_SEC 30 // Sample GPS every 30 seconds

// #define ESP32_RTOS 
// #include "OTA.h"
// for debugging puposes, print the reset reason on startup
#include <esp_system.h>
const char* resetReasonToStr(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_UNKNOWN:   return "UNKNOWN";
    case ESP_RST_POWERON:   return "POWERON";
    case ESP_RST_EXT:       return "EXT";
    case ESP_RST_SW:        return "SW";
    case ESP_RST_PANIC:     return "PANIC";
    case ESP_RST_INT_WDT:   return "INT_WDT";
    case ESP_RST_TASK_WDT:  return "TASK_WDT";
    case ESP_RST_WDT:       return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "OTHER";
  }
}


TaskHandle_t gpsTaskHandle = NULL;
TaskHandle_t uiTaskHandle = NULL;
TaskHandle_t buttonTaskHandle = NULL;
TaskHandle_t WiFiTaskHandle = NULL;
TaskHandle_t gpsSamplerTaskHandle = NULL;
TaskHandle_t uploadTaskHandle = NULL;
TaskHandle_t batteryTaskHandle = NULL;
TaskHandle_t loraTaskHandle = NULL;
volatile bool uiNextFrameRequested = false;



void gpsTask(void *pvParameters) {
  // Initialize GPS
  gpsInit();
  while (true) {
    gpsUpdate();
    vTaskDelay(100 / portTICK_PERIOD_MS); // Adjust the delay as needed
  }
}

void uiTask(void *pvParameters) {
  // Initialize OLED UI
  uiInit();
  while (true) {
    if (uiNextFrameRequested) {
      uiNextFrameRequested = false;
      ui.nextFrame();
    }

    int remainingTimeBudget = ui.update();
    if (remainingTimeBudget < 0) {
      Serial.println("UI update over budget!");
      remainingTimeBudget = 0; // Avoid negative delay
    }
    vTaskDelay(remainingTimeBudget / portTICK_PERIOD_MS); // Adjust the delay as needed
  }
}

void buttonTask(void *pvParameters) {
  buttonInit();
  while (true) {
    handleButtonPress();
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void WiFiTask(void *pvParameters) {
  WiFiInit();
  while (true) {
    updateWiFi();
    vTaskDelay(500 / portTICK_PERIOD_MS);  // Check more frequently for async scan completion
  }
}

void gpsSamplerTask(void *pvParameters) {
  initTrackStore(TRACK_CAPACITY); // Initialize track storage (ring buffer)
  TickType_t lastWake = xTaskGetTickCount();
  const TickType_t sampleInterval = pdMS_TO_TICKS(GPS_SAMPLING_RATE_SEC * 1000); // Sample every GPS_SAMPLING_RATE_SEC seconds
  while (true) {
    vTaskDelayUntil(&lastWake, sampleInterval); // Wait for the next sample time
    if (!gpsHasLocation()) continue; // Skip if no valid location fix
    sampleGPSFix();
  }
}

void uploadTask(void *pvParameters) {
  FixRec batch[MAX_UPLOAD_BATCH_SIZE];
  while (true) {
    vTaskDelay(UPLOAD_INTERVAL_MS / portTICK_PERIOD_MS);
    uploadBatchOverWiFi(batch);
    // Serial.printf("Task1 Stack Free: %u words\n", uxTaskGetStackHighWaterMark(NULL));
    // Serial.printf("Stack free: %u bytes\n", uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t));
  }
}

void batteryTask(void *pvParameters) {
  batteryInit();
  while (true) {
    batteryUpdate();
    vTaskDelay(10000 / portTICK_PERIOD_MS); // Update every 10 seconds
  }
}

void loraTask(void *pvParameters) {
  loraInit();
  bool wasWifiConnected = false;
  
  while (true) {
    // Manage LoRa radio power based on WiFi connection state
    if (wifiConnected && !wasWifiConnected) {
      // WiFi just connected - shutdown LoRa radio to save power
      loraStop();
      wasWifiConnected = true;
    } 
    else if (!wifiConnected && wasWifiConnected) {
      // WiFi just disconnected - wake LoRa radio for transmission
      loraResume();
      wasWifiConnected = false;
    }
    
    // Only update LoRa if WiFi is not connected (saves CPU cycles)
    if (!wifiConnected) {
      loraUpdate();
    }
    
    vTaskDelay(100 / portTICK_PERIOD_MS); // Check every 100ms
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  delay(2000); // Wait for Serial to initialize
  Serial.println("Program started. Setting up...");
  Serial.printf("Reset reason: %s\n", resetReasonToStr(esp_reset_reason()));

  // V4-SPECIFIC: Power on external PA (GC1109) BEFORE any LoRa initialization
  pinMode(7, OUTPUT);   // LORA_PA_POWER (VFEM)
  pinMode(2, OUTPUT);   // LORA_PA_EN (CSD)
  pinMode(46, OUTPUT);  // LORA_PA_TX_EN (CPS)
  digitalWrite(7, HIGH);
  digitalWrite(2, HIGH);
  digitalWrite(46, HIGH);
  delay(100);
  Serial.println("V4: External PA powered on (GPIO 7, 2, 46)");

  // OTA setup (optional, only if you want OTA updates)
  // const char* mySSID = SSID_HOME;
  // const char* myPASSWORD = PASS_HOME;
  // setupOTA("TemplateSketch", mySSID, myPASSWORD);

  // Create tasks
  xTaskCreate(gpsTask, "GPS Task", 10000, NULL, 1, &gpsTaskHandle);
  xTaskCreate(uiTask, "UI Task", 10000, NULL, 1, &uiTaskHandle);
  xTaskCreate(buttonTask, "Button Task", 10000, NULL, 1, &buttonTaskHandle);
  xTaskCreatePinnedToCore(WiFiTask, "WiFi Task", 16384, NULL, 1, &WiFiTaskHandle, 1);  // 16KB for WiFi operations
  xTaskCreate(gpsSamplerTask, "GPS Sampler Task", 10000, NULL, 1, &gpsSamplerTaskHandle);
  xTaskCreatePinnedToCore(uploadTask, "Upload Task", 10000, NULL, 1, &uploadTaskHandle, 1);
  xTaskCreate(batteryTask, "Battery Task", 4096, NULL, 1, &batteryTaskHandle);
  xTaskCreatePinnedToCore(loraTask, "LoRa Task", 8192, NULL, 1, &loraTaskHandle, 1);  // 8KB for LoRa radio
}

void loop() {
  // The loop is empty because all the work is done in tasks
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
