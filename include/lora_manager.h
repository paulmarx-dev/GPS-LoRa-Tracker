#ifndef LORA_MANAGER_H
#define LORA_MANAGER_H

#include <Arduino.h>

/**
 * Initialize LoRa radio and start transmission
 * Call once at startup
 */
void loraInit();

/**
 * Periodic LoRa update (call from loraTask)
 * Handles transmission of GPS data
 */
void loraUpdate();

/**
 * Send test payload with GPS data
 */
void sendPayload(int32_t ts, int32_t latE7, int32_t lonE7, uint8_t bat);

void checkAndSend();

/**
 * Shutdown LoRa radio to save power (when WiFi connected)
 */
void loraStop();

/**
 * Resume LoRa radio operation (when WiFi disconnected)
 */
void loraResume();

/**
 * Get last LoRa transmission timestamp (milliseconds)
 */
uint32_t getLastLoraTxMs();

/**
 * Get total LoRa payloads transmitted
 */
uint32_t getLoraTxCount();

/**
 * Check if LoRa transmission is currently active
 */
bool isLoraTxActive();

#endif // LORA_MANAGER_H