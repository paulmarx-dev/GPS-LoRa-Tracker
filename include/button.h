#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>
#include "track_storage.h"
#include "gps_sampler.h"

#define BUTTON_PIN 0
#define SHORT_PRESS_TIME 500    // 500 milliseconds
#define LONG_PRESS_TIME  2000   // 2000 milliseconds
#define VERY_LONG_PRESS_TIME 5000 // 5000 milliseconds
#define DEBOUNCE_DELAY 50       // 50 milliseconds

// External variables for button state
extern bool autoTransitionEnabled;
extern String transitionMode;  // "M" for manual, "A" for auto-transition
extern volatile bool uiNextFrameRequested;

// External task handle
extern TaskHandle_t buttonTaskHandle;

/**
 * Initialize button and setup GPIO
 */
void buttonInit();

/**
 * Handle button press detection and state changes
 */
void handleButtonPress();

/**
 * Button task for FreeRTOS
 */
void buttonTask(void *pvParameters);

void uploadBatchOverWiFi(FixRec batch[60], int &retFlag);

void sampleGPSFix();

#endif // BUTTON_H
