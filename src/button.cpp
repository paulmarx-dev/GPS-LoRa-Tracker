#include "button.h"
#include "oled.h"

// Button state variables
volatile uint32_t lastInterruptTime = 0;
volatile int lastState = HIGH;
volatile int currentState = HIGH;
volatile unsigned long pressedTime = 0;
volatile unsigned long releasedTime = 0;

// Global button state
bool autoTransitionEnabled = false;
String transitionMode = "M";  // "M" for manual, "A" for auto-transition
bool screenOn = true; 
extern TaskHandle_t uiTaskHandle; // Declare external task handle for UI task
extern volatile bool uiNextFrameRequested;

void handleButtonPress() {
  uint32_t currentTime = millis();
  if (currentTime - lastInterruptTime < DEBOUNCE_DELAY) {
    return; // Ignore if within debounce period
  }
  lastInterruptTime = currentTime;

  currentState = digitalRead(BUTTON_PIN);
  long pressDuration = 0; // Initialize pressDuration to avoid uninitialized variable warning

  if (lastState == HIGH && currentState == LOW) {      // button is pressed
    //Serial.println("Button pressed"); // Debug: Print when button is pressed
    pressedTime = millis();
  } else if (lastState == LOW && currentState == HIGH) { // button is released
    //Serial.println("Button released"); // Debug: Print when button is released
    releasedTime = millis();
    pressDuration = releasedTime - pressedTime;
  }

  if (pressDuration > DEBOUNCE_DELAY && pressDuration < SHORT_PRESS_TIME) {
    Serial.println("A short press is detected");
    bool wasScreenOff = !screenOn;  // Remember screen state before modification
    if (wasScreenOff) {
        Serial.println("Waking screen, not switching frame");
        screenOn = true;
        oledVextON();  // Ensure OLED power is on
        vTaskResume(uiTaskHandle); // Resume UI task if it was suspended
    } else {
        Serial.println("Switching to next UI frame");
        uiNextFrameRequested = true;
    }
  }
  
  if (pressDuration > LONG_PRESS_TIME && pressDuration < VERY_LONG_PRESS_TIME) {
    Serial.println("A long press is detected");
    if (autoTransitionEnabled) {
      autoTransitionEnabled = !autoTransitionEnabled; // Toggle auto-transition state
      ui.disableAutoTransition();
      transitionMode = "M";
    } else {
      autoTransitionEnabled = !autoTransitionEnabled; // Toggle auto-transition state
      ui.enableAutoTransition();
      transitionMode = "A";
    }
  }

  if (pressDuration >= VERY_LONG_PRESS_TIME) {
    Serial.println("A very long press is detected, resetting UI to first frame.");
    if (screenOn) {
      screenOn = false;
      vTaskSuspend(uiTaskHandle); // Suspend UI task to save power
      oledVextOFF(); // Turn off OLED power
    } 
    // else {
    //   screenOn = true;
    //   vTaskResume(uiTaskHandle); // Resume UI task
    //   oledVextON();  // Turn on OLED power
    // }
    
  }

  // save the last state
  lastState = currentState;
  pressDuration = 0; // reset press duration
}

void buttonInit() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}


