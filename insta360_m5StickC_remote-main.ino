/*
By Cameron Coward
serialhobbyism.com
8/15/25

Insta360 Remote for M5Stack devices. Tested on original M5StickC and M5StickC Plus2. Should also work on M5StickC Plus.

Tested on Insta360 X5. Should work on other X-series models and RS.

Supports "wake" of camera.

Make sure you set REMOTE_IDENTIFIER below. Just select three alphanumeric characters of your choice to prevent interference with multiple remotes.

Make sure you have the other files in the same folder: config.h, icons.h, camera.h, ble_handlers.h, ui.h, and commands.h
*/


// #include <M5StickC.h>
#include <M5Unified.h>
#include "BLEDevice.h"
#include "BLEUtils.h"
#include "BLEServer.h"
#include "BLE2902.h"
#include "Preferences.h"

// *** CONFIGURE YOUR UNIQUE REMOTE IDENTIFIER HERE ***
// Change this 3-character identifier for each remote to prevent interference
// Examples: "001", "A01", "XYZ", "Bob", etc.
const char REMOTE_IDENTIFIER[4] = "A01";  // Maximum 3 characters + null terminator

// Calculate unique GPIO delay based on REMOTE_IDENTIFIER (0-100ms)
int calculateGPIODelay() {
  int hash = 0;
  for (int i = 0; i < 3 && REMOTE_IDENTIFIER[i] != '\0'; i++) {
    hash += (REMOTE_IDENTIFIER[i] * (i + 1));  // Weight by position
  }
  return hash % 101;  // Returns 0-100ms
}

// GPIO delay for this remote (calculated once at startup)
int gpioDelay = 0;

// Recording state
bool isRecording = false;
unsigned long recordingStartTime = 0;

// Smart Wake state
bool pendingRecordAfterWake = false;
unsigned long wakeRequestTime = 0;

// Include all module headers in correct order
#include "config.h"
#include "icons.h"
#include "camera.h"

// Forward declarations for cross-dependencies
void updateDisplay();
void drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color);
void showBottomStatus(const char* text, uint16_t color);
void showCenteredMessage(const char* line1, const char* line2, uint16_t color);
void applyLayoutRotation();
void setNormalAdvertising();
void setWakeAdvertising(uint8_t* wakePayload);
void sendCommand(uint8_t* command, size_t length, const char* commandName);
void executeShutter();
void executeSleep();
void executeWake();
void executeSwitchMode();
void executeScreenOff();
void connectCamera1();
void connectCamera2();
void showNotConnectedMessage();
void showNoCameraMessage();
void checkGPIOPins();

// Now include the implementation headers
#include "ble_handlers.h"
#include "ui.h"
#include "commands.h"

void setup() {
  M5.begin();
  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(1);
  detectDeviceAndSetScale();

  Serial.begin(115200);
  Serial.println("M5StickC Insta360 Camera Remote");
  Serial.print("Remote ID: ");
  Serial.println(REMOTE_IDENTIFIER);

  // Calculate unique GPIO delay for this remote
  gpioDelay = calculateGPIODelay();
  Serial.print("GPIO delay for this remote: ");
  Serial.print(gpioDelay);
  Serial.println("ms");

  // Record startup time for GPIO delay
  startupTime = millis();

  // Setup GPIO pins - G0 uses hardware pullup, others use pulldown
  pinMode(SHUTTER_PIN, INPUT);           // G0 has hardware pullup - trigger on LOW (to GND)
  pinMode(SLEEP_PIN, INPUT_PULLDOWN);    // G26 for Sleep (#5) - trigger on HIGH (to 3.3V)
  pinMode(WAKE_PIN, INPUT_PULLDOWN);     // G36 for Wake (#6) - trigger on HIGH (to 3.3V)

  Serial.println("GPIO pins configured:");
  Serial.println("G0 (Shutter) - INPUT (hardware pullup, trigger on GND)");
  Serial.println("G26 (Sleep) - INPUT_PULLDOWN (trigger on 3.3V)");
  Serial.println("G36 (Wake) - INPUT_PULLDOWN (trigger on 3.3V)");
  Serial.println("GPIO input disabled for 2 seconds after startup...");

  // Load saved cameras
  loadAllCameras();
  
  // Apply saved layout preference (Orientation)
  applyLayoutRotation();

  // Initialize BLE
  // Use exact name to match official remotes (Ace Pro 2 requires exact match)
  String deviceName = "Insta360 GPS Remote";
  
  // Set custom handler to capture GATTS_IF for unicast support
  BLEDevice::setCustomGattsHandler(myGattsHandler);
  BLEDevice::init(deviceName.c_str());

  // Create BLE Scanner for camera detection
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyScanCallbacks());
  pBLEScan->setActiveScan(true); // Active scan uses more power but gets names
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  pService = pServer->createService(GPS_REMOTE_SERVICE_UUID);

  // Create characteristics
  pWriteCharacteristic = pService->createCharacteristic(
                      GPS_REMOTE_WRITE_CHAR_UUID,
                      BLECharacteristic::PROPERTY_WRITE
                    );
  pWriteCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

  pNotifyCharacteristic = pService->createCharacteristic(
                      GPS_REMOTE_NOTIFY_CHAR_UUID,
                      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
                    );
  pNotifyCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start with normal advertising
  setNormalAdvertising();

  Serial.println("Ready!");
  updateDisplay();
}

// Flag to track if long press was handled to avoid double-triggering short press
bool ignoreNextRelease = false;

void loop() {
  M5.update();

  bool anyConnected = (camera1Connected || camera2Connected) && pServer && (pServer->getConnectedCount() > 0);

  // Check GPIO pins for external button presses
  checkGPIOPins();

  // --- Smart Wake & Record Monitoring ---
  if (pendingRecordAfterWake) {
      int expected = 0;
      if (camera1.isValid) expected++;
      if (camera2.isValid) expected++;
      
      int connected = 0;
      if (camera1Connected) connected++;
      if (camera2Connected) connected++;
      
      // Check if we hit our target
      if (connected >= expected && expected > 0) {
          Serial.printf("Smart Wake Trigger: Connected %d / Expected %d\n", connected, expected);
          Serial.println("Smart Wake: All cameras connected! Starting Record...");
          pendingRecordAfterWake = false;
          delay(2000); // Give a moment for connection stability
          executeShutter();
          updateDisplay();
      } 
      // Check timeout (30s to be safe, wake takes time + connection takes time)
      else if (millis() - wakeRequestTime > 30000) {
          Serial.println("Smart Wake: Timeout waiting for connections.");
          pendingRecordAfterWake = false;
          showCenteredMessage("Wake Failed", "Timeout", RED);
          delay(2000);
          updateDisplay();
      }
  }

  // --- Recording State Management (Timeout Logic) ---
  // If a camera hasn't sent a timer packet for > 5.0 seconds, assume it stopped recording.
  unsigned long now = millis();
  
  if (camera1Connected && camera1.isRecording) {
    if (now - camera1.lastTimerTime > 5000) {
      camera1.isRecording = false;
      Serial.println("Camera 1 recording timeout -> Stopped");
      updateDisplay();
    }
  }
  
  if (camera2Connected && camera2.isRecording) {
    if (now - camera2.lastTimerTime > 5000) {
      camera2.isRecording = false;
      Serial.println("Camera 2 recording timeout -> Stopped");
      updateDisplay();
    }
  }

  // Sync global `isRecording` with actual camera states (OR logic)
  bool actualRecording = (camera1Connected && camera1.isRecording) || (camera2Connected && camera2.isRecording);
  
  // Handle state transitions
  if (actualRecording != isRecording) {
      // State changed!
      isRecording = actualRecording;
      if (isRecording) {
          // We just started recording (or detected it)
          recordingStartTime = millis(); 
      } else {
          // We just stopped
          updateDisplay(); // Force redraw to remove timer immediately
      }
  }

  // Update recording timer on screen 0 (Dashboard)
  static unsigned long lastTimerUpdate = 0;
  if (currentScreen == 0 && isRecording && millis() - lastTimerUpdate > 1000) {
    lastTimerUpdate = millis();
    updateDashboardTimer(); // Use optimized redraw
  }

  // Handle UI updates requested by BLE callbacks
  if (updateScreenRequested) {
    updateScreenRequested = false;
    // Simple redraw to show updated status/names
    updateDisplay();
  }

  // Button B - Navigation
  if (M5.BtnB.wasReleased()) {
    if (currentScreen == 0) {
        // Go to Pairing Menu
        currentScreen = 1;
        pairingMenuSelection = 0; // Reset to first option
    } else if (currentScreen == 1) {
        // Cycle Pairing Menu Options (4 items now)
        pairingMenuSelection = (pairingMenuSelection + 1) % 4;
    }
    updateDisplay();
  }

  // Handle Button A (Long Press = Power On/Off, Short Press = Shutter/Action)
  // Check for Long Press (1000ms) - ONLY on Dashboard
  if (currentScreen == 0 && M5.BtnA.pressedFor(1000) && !ignoreNextRelease) {
    Serial.println("Button A Long Press detected!");
    ignoreNextRelease = true; // Prevent short press trigger on release
    
    // Logic: If cameras connected -> Sleep. If not -> Wake.
    if (anyConnected) {
      executeSleep();
      // Feedback in purple bar
      showBottomStatus("SLEEPING...", PURPLE);
      delay(1000);
    } else {
      executeWake();
      // executeWake shows its own UI feedback
    }
    updateDisplay();
  }

  // Check for Short Press (Release)
  if (M5.BtnA.wasReleased()) {
    if (ignoreNextRelease) {
      // This was the release of a long press - reset flag and do nothing
      ignoreNextRelease = false;
    } else {
      // Short press logic based on screen
      if (currentScreen == 0) {
          // Dashboard: Shutter
          if (!anyConnected) {
            // SMART WAKE & RECORD
            Serial.println("Smart Wake initiated...");
            pendingRecordAfterWake = true;
            wakeRequestTime = millis();
            executeWake(); // Blocking call (~6s for two cams)
            
            // After wake signal sent, show waiting status
            showCenteredMessage("Waiting for", "Connection...", YELLOW);
          } else {
            executeShutter();
            updateDisplay();
          }
      } else if (currentScreen == 1) {
          // Pairing Menu: Select Option
          if (pairingMenuSelection == 0) {
              connectCamera1();
              currentScreen = 0; // Return to dash after pairing attempt
          } else if (pairingMenuSelection == 1) {
              connectCamera2();
              currentScreen = 0;
          } else if (pairingMenuSelection == 2) {
              // Toggle Layout
              saveLayoutPreference(!isVerticalLayout);
              applyLayoutRotation(); // Rotate screen immediately
              // Stay in menu to confirm? Or return? Let's stay so they see the change text.
              updateDisplay();
          } else {
              // Back
              currentScreen = 0;
          }
          updateDisplay();
      }
    }
  }

  delay(50);
}
