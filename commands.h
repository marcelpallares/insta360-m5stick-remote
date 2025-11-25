/*
 * commands.h
 * Camera command execution functions
 */

#ifndef COMMANDS_H
#define COMMANDS_H

// Forward declarations needed
void executeShutter();
void executeSleep();
void executeWake();

void connectCamera(int cameraNum) {
  Serial.print("Starting camera ");
  Serial.print(cameraNum);
  Serial.println(" pairing process");

  // Set pairing mode for this camera slot
  pairingCameraSlot = cameraNum;

  // Reset detection variables
  detectedCameraName = "";
  detectedCameraAddress = "";

  M5.Lcd.fillScreen(BLACK);
  // Draw pairing icon
  drawBitmap(64, 15, pairing_icon, 32, 32, ICON_CYAN);
  M5.Lcd.setCursor(25, 50);
  M5.Lcd.setTextColor(YELLOW);
  M5.Lcd.print("PAIRING CAM ");
  M5.Lcd.print(cameraNum);
  M5.Lcd.setCursor(25, 65);
  M5.Lcd.setTextColor(CYAN);
  M5.Lcd.setTextSize(1);
  M5.Lcd.println("B:Cancel");
  delay(2000);

  // Start scanning for cameras
  pairingMode = true;
  Serial.println("Starting scan for Insta360 cameras");

  M5.Lcd.fillScreen(BLACK);
  // Draw pairing icon again
  drawBitmap(64, 10, pairing_icon, 32, 32, ICON_CYAN);
  M5.Lcd.setCursor(35, 45);
  M5.Lcd.setTextColor(YELLOW);
  M5.Lcd.println("Scanning...");
  M5.Lcd.setCursor(40, 65);
  M5.Lcd.setTextColor(CYAN);
  M5.Lcd.println("B:Cancel");

  // Start continuous scanning
  if (pBLEScan) {
    pBLEScan->start(0, nullptr, false); // Continuous scan
  }

  // Ensure advertising is on
  setNormalAdvertising();

  // Wait for camera to be detected and connect
  unsigned long startTime = millis();
  while (pairingMode && millis() - startTime < 30000) { // 30 second timeout
    M5.update();

    if (M5.BtnB.wasReleased()) {
      Serial.println("Pairing cancelled by user");
      pairingMode = false;
      pairingCameraSlot = 0;
      if (pBLEScan) {
        pBLEScan->stop();
      }
      updateDisplay();
      return;
    }

    // Update display with detected camera if found
    static String lastDetected = "";
    if (detectedCameraName.length() > 0 && detectedCameraName != lastDetected) {
      lastDetected = detectedCameraName;
      M5.Lcd.fillRect(15, 45, 130, 15, BLACK);
      M5.Lcd.setCursor(35, 45);
      M5.Lcd.setTextColor(BLUE);
      M5.Lcd.print("Found!");
    }

    delay(100);
  }

  // Timeout - stop scanning
  if (pairingMode) {
    pairingMode = false;
    pairingCameraSlot = 0;
    if (pBLEScan) {
      pBLEScan->stop();
    }

    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(40, 30);
    M5.Lcd.setTextColor(YELLOW);
    M5.Lcd.println("Timeout");
    M5.Lcd.setCursor(35, 45);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.println("Try again");
    delay(2000);
  }

  updateDisplay();
}

// Wrapper functions for each camera
void connectCamera1() {
  connectCamera(1);
}

void connectCamera2() {
  connectCamera(2);
}

void executeShutter() {
  // Sync Check Logic
  bool c1Rec = camera1Connected && camera1.isRecording;
  bool c2Rec = camera2Connected && camera2.isRecording;
  
  if (c1Rec && !c2Rec) {
     // Camera 1 is recording, Camera 2 is stopped.
     // We want to STOP Camera 1 to sync them (both stopped).
     // Send Unicast Toggle to Camera 1 ONLY.
     if (camera1Connected) {
       Serial.println("Syncing: Stopping Cam 1 to match Cam 2");
       sendUnicastCommand(camera1.connId, SHUTTER_CMD, sizeof(SHUTTER_CMD), "SHUTTER (U1)");
     }
  } 
  else if (!c1Rec && c2Rec) {
     // Camera 2 is recording, Camera 1 is stopped.
     // We want to STOP Camera 2 to sync them.
     // Send Unicast Toggle to Camera 2 ONLY.
     if (camera2Connected) {
       Serial.println("Syncing: Stopping Cam 2 to match Cam 1");
       sendUnicastCommand(camera2.connId, SHUTTER_CMD, sizeof(SHUTTER_CMD), "SHUTTER (U2)");
     }
  } 
  else {
     // Both are in same state (Both Rec or Both Stop).
     // Standard Broadcast Toggle.
     sendCommand(SHUTTER_CMD, sizeof(SHUTTER_CMD), "SHUTTER");
  }
}

void executeSwitchMode() {
  sendCommand(MODE_CMD, sizeof(MODE_CMD), "MODE");
}

void executeScreenOff() {
  sendCommand(TOGGLE_SCREEN_CMD, sizeof(TOGGLE_SCREEN_CMD), "SCREEN");
}

void executeSleep() {
  sendCommand(POWER_OFF_CMD, sizeof(POWER_OFF_CMD), "SLEEP");
}

void executeWake() {
  // Check if at least one camera is saved
  if (!camera1.isValid && !camera2.isValid) {
    showCenteredMessage("No camera", "Saved!", RED);
    delay(2000);
    updateDisplay();
    return;
  }

  int wakingCount = 0;
  if (camera1.isValid) wakingCount++;
  if (camera2.isValid) wakingCount++;

  // Wake camera 1 if saved
  if (camera1.isValid) {
    showCenteredMessage("Waking 1...", camera1.name, YELLOW);
    setWakeAdvertising(camera1.wakePayload);
    delay(3000); // Send wake signal for 3 seconds
  }

  // Wake camera 2 if saved
  if (camera2.isValid) {
    showCenteredMessage("Waking 2...", camera2.name, YELLOW);
    setWakeAdvertising(camera2.wakePayload);
    delay(3000); // Send wake signal for 3 seconds
  }

  setNormalAdvertising();

  showCenteredMessage("Wake Signal", "SENT!", BLUE);
  delay(1500);
  updateDisplay();
}

#endif // COMMANDS_H