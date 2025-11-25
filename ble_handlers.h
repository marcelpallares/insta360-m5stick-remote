/*
 * ble_handlers.h
 * BLE callbacks, advertising, and communication functions
 */

#ifndef BLE_HANDLERS_H
#define BLE_HANDLERS_H

// BLE variables
BLEServer* pServer = nullptr;
BLEService* pService = nullptr;
BLECharacteristic* pWriteCharacteristic = nullptr;
BLECharacteristic* pNotifyCharacteristic = nullptr;
BLEScan* pBLEScan = nullptr;

// Global GATT Interface ID for unicast
uint16_t g_gattsIf = 0;

// Custom GATT Handler to capture the Interface ID
void myGattsHandler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    if (event == ESP_GATTS_REG_EVT) {
        g_gattsIf = gatts_if;
        Serial.print("Captured GATTS IF: ");
        Serial.println(g_gattsIf);
    }
}

// UI Request flags (handled in loop)
volatile bool updateScreenRequested = false;
volatile int connectionMessageId = 0; // 0=None, 1=Cam1, 2=Cam2, 3=Unknown, 4=Paired
volatile int pairingSlotCompleted = 0;

// Forward declarations
void setNormalAdvertising();
void setWakeAdvertising(uint8_t* wakePayload);

// BLE Scan callback to capture camera info during pairing mode
class MyScanCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      if (!pairingMode) return; // Only process during pairing mode

      // Look for Insta360 cameras
      if (advertisedDevice.haveName()) {
        String deviceName = advertisedDevice.getName().c_str();
        String deviceAddress = advertisedDevice.getAddress().toString().c_str();

        Serial.print("Scan found: ");
        Serial.print(deviceName);
        Serial.print(" @ ");
        Serial.println(deviceAddress);

        // Check if this is an Insta360 camera
        if (deviceName.startsWith("X3 ") ||
            deviceName.startsWith("X4 ") ||
            deviceName.startsWith("X5 ") ||
            deviceName.startsWith("RS ") ||
            deviceName.startsWith("ONE ") ||
            deviceName.startsWith("Ace ") ||
            deviceName.startsWith("ACE ")) {

          // Found an Insta360 camera - save its info
          detectedCameraName = deviceName;
          detectedCameraAddress = deviceAddress;

          Serial.print("Found Insta360 camera: ");
          Serial.println(detectedCameraName);
        }
      }
    }
};

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param) {
      // Get the connected device's address
      char addressStr[18];
      sprintf(addressStr, "%02x:%02x:%02x:%02x:%02x:%02x",
              param->connect.remote_bda[0],
              param->connect.remote_bda[1],
              param->connect.remote_bda[2],
              param->connect.remote_bda[3],
              param->connect.remote_bda[4],
              param->connect.remote_bda[5]);
      String connectedAddress = String(addressStr);
      uint16_t connId = param->connect.conn_id;

      Serial.print("Device connected from address: ");
      Serial.println(connectedAddress);
      Serial.print("Connection ID: ");
      Serial.println(connId);

      // Check if we're in pairing mode and have detected a camera
      if (pairingMode && detectedCameraName.length() > 0 && pairingCameraSlot > 0) {
        // Stop scanning
        if (pBLEScan) {
          pBLEScan->stop();
        }
        pairingMode = false;

        Serial.print("Pairing camera to slot ");
        Serial.print(pairingCameraSlot);
        Serial.print(": ");
        Serial.println(detectedCameraName);

        // Validate camera name format
        bool validFormat = false;
        if (detectedCameraName.length() >= 9) { // "X5 " + 6 chars minimum
          int spaceIndex = detectedCameraName.indexOf(' ');
          if (spaceIndex > 0 && detectedCameraName.length() - spaceIndex > 6) {
            validFormat = true;
          }
        }

        if (validFormat) {
          // Save to the appropriate slot
          saveCamera(pairingCameraSlot, detectedCameraName, detectedCameraAddress);

          // Mark as connected
          if (pairingCameraSlot == 1) {
            camera1Connected = true;
            camera1.connId = connId;
            camera1ConnectedAddress = connectedAddress;
          } else {
            camera2Connected = true;
            camera2.connId = connId;
            camera2ConnectedAddress = connectedAddress;
          }
          
          pairingSlotCompleted = pairingCameraSlot;
          connectionMessageId = 4; // Paired message
          pairingCameraSlot = 0;  // Reset pairing slot
        } else {
           // Invalid format handling - should be handled in main loop ideally, 
           // but for now we just disconnect.
           pServer->disconnect(connId);
        }
      } else if (pairingMode) {
        // In pairing mode but no camera detected yet (direct connect?)
        // This is ambiguous, usually scan finds it first.
        // We'll treat it as unknown/failed for now.
        pairingMode = false;
        pairingCameraSlot = 0;
        if (pBLEScan) {
          pBLEScan->stop();
        }
        pServer->disconnect(connId);
      } else {
        // Not in pairing mode - check if this is a known camera reconnecting
        bool knownCamera = false;

        // Check if this is camera1
        bool matched1 = false;
        if (camera1.isValid && connectedAddress.equalsIgnoreCase(camera1.address)) {
          camera1Connected = true;
          camera1.connId = connId;
          camera1ConnectedAddress = connectedAddress;
          Serial.print("Camera 1 reconnected: ");
          Serial.println(camera1.name);
          knownCamera = true;
          connectionMessageId = 1;
          matched1 = true;
        }

        // Check if this is camera2
        // Only match camera 2 if it wasn't already matched to camera 1, 
        // OR if it is matched but we want to ensure unique assignment (prevent double-dot for single cam)
        // If user REALLY wants same cam on both, we'll allow it but prioritize slot 1 visually if we had to choose.
        // But the issue reported is confusing double-dots. 
        // We will prevent Slot 2 from activating if Slot 1 already claimed this Connection ID, 
        // UNLESS the addresses are explicitly different (which shouldn't happen if they matched the same string).
        // Actually, if the addresses are identical, we should probably only light up one.
        
        bool matched2 = false;
        if (camera2.isValid && connectedAddress.equalsIgnoreCase(camera2.address)) {
           if (!matched1) {
              camera2Connected = true;
              camera2.connId = connId;
              camera2ConnectedAddress = connectedAddress;
              Serial.print("Camera 2 reconnected: ");
              Serial.println(camera2.name);
              knownCamera = true;
              connectionMessageId = 2;
              matched2 = true;
           } else {
              Serial.println("Device matches both slots - skipping Slot 2 activation to prevent duplicate status.");
           }
        }

        if (!knownCamera) {
          Serial.println("Unknown camera connected");
          connectionMessageId = 3;
          // Save unknown address to global for display
          detectedCameraAddress = connectedAddress; 
          // Disconnect unknown camera
          pServer->disconnect(connId);
        }
      }

      updateScreenRequested = true;
      
      // CRITICAL FIX: Restart advertising to allow other cameras to connect
      // We only restart if we are not in wake mode (wake mode has special advertising)
      // and not in pairing mode (pairing flow handles its own state)
      if (!wakeMode && !pairingMode) {
        // Small delay to ensure stack stability before restarting advertising
        // Note: blocking in callback is bad, but strict restart sometimes needs a tick.
        // Better to just call start().
        BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
        if (pAdvertising) {
            pAdvertising->start();
            Serial.println("Advertising restarted for multi-connection support");
        }
      }
    }

    void onDisconnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param) {
      uint16_t connId = param->disconnect.conn_id;
      Serial.print("Camera disconnected, ID: ");
      Serial.println(connId);

      bool changed = false;

      // Check which camera disconnected based on connId
      if (camera1Connected && camera1.connId == connId) {
        camera1Connected = false;
        camera1.connId = 0xFFFF;
        camera1ConnectedAddress = "";
        Serial.println("Camera 1 disconnected");
        changed = true;
      } 
      
      if (camera2Connected && camera2.connId == connId) {
        camera2Connected = false;
        camera2.connId = 0xFFFF;
        camera2ConnectedAddress = "";
        Serial.println("Camera 2 disconnected");
        changed = true;
      }

      // Fallback: Only wipe if we are SURE no one is connected.
      // Using getConnectedCount() immediately after disconnect callback might be race-prone.
      // Safer to rely on ID matching. 
      // If ID matching failed (changed == false), it means we disconnected something we didn't track as active.
      // So we shouldn't wipe the active ones!
      if (!changed) {
         Serial.println("Disconnected device was not tracked as active camera.");
      }

      if (changed) {
        updateScreenRequested = true;
      }

      // Return to normal advertising to allow reconnection
      if (!wakeMode && !pairingMode) {
        // Ensure advertising is running
        BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
        if (pAdvertising) {
             pAdvertising->start();
        }
      }
    }
    
    void onDisconnect(BLEServer* pServer) {
        // This should not be called if the param version is available, 
        // but explicitly keeping it empty to avoid abstract class issues.
    }
};

class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic, esp_ble_gatts_cb_param_t* param) {
      uint16_t connId = param->write.conn_id;
      uint8_t* data = param->write.value;
      size_t len = param->write.len;

      if (len > 0) {
        // Heuristic: Timer packets are usually length 19 or 20 AND contain ASCII digits/colons.
        // We scan for ':' (0x3A) to confirm it's a timer packet.
        
        bool isTimerPacket = false;
        if (len >= 18) {
            for (int i = 0; i < len; i++) {
                if (data[i] == 0x3A) { // Found ':'
                    isTimerPacket = true;
                    break;
                }
            }
        }

        if (isTimerPacket) {
           if (camera1Connected && camera1.connId == connId) {
               // Only request full screen update if state CHANGES (Start Recording)
               if (!camera1.isRecording) {
                   camera1.isRecording = true;
                   updateScreenRequested = true; 
               }
               camera1.lastTimerTime = millis();
           } 
           else if (camera2Connected && camera2.connId == connId) {
               // Only request full screen update if state CHANGES (Start Recording)
               if (!camera2.isRecording) {
                   camera2.isRecording = true;
                   updateScreenRequested = true;
               }
               camera2.lastTimerTime = millis();
           }
        }
      }
    }
    
    void onWrite(BLECharacteristic* pCharacteristic) {}
};

void setWakeAdvertising(uint8_t* wakePayload) {
  Serial.print("Setting wake advertising with payload: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X ", wakePayload[i]);
  }
  Serial.println();

  // Stop current advertising
  BLEDevice::stopAdvertising();
  delay(100);

  // Create manufacturer data for wake-up (iBeacon format)
  uint8_t manufacturerData[26];

  // Apple company ID and iBeacon format
  manufacturerData[0] = 0x4c;  // Apple company ID (low byte)
  manufacturerData[1] = 0x00;  // Apple company ID (high byte)
  manufacturerData[2] = 0x02;  // iBeacon format identifier
  manufacturerData[3] = 0x15;  // iBeacon format identifier

  // iBeacon UUID (16 bytes) - specific pattern for Insta360 wake
  manufacturerData[4] = 0x09;
  manufacturerData[5] = 0x4f;
  manufacturerData[6] = 0x52;
  manufacturerData[7] = 0x42;
  manufacturerData[8] = 0x49;
  manufacturerData[9] = 0x54;
  manufacturerData[10] = 0x09;
  manufacturerData[11] = 0xff;
  manufacturerData[12] = 0x0f;
  manufacturerData[13] = 0x00;

  // Camera-specific wake payload (6 bytes)
  memcpy(&manufacturerData[14], wakePayload, 6);

  // Additional iBeacon data
  manufacturerData[20] = 0x00;  // Major (high byte)
  manufacturerData[21] = 0x00;  // Major (low byte)
  manufacturerData[22] = 0x00;  // Minor (high byte)
  manufacturerData[23] = 0x00;  // Minor (low byte)
  manufacturerData[24] = 0xe4;  // TX Power
  manufacturerData[25] = 0x01;  // Additional byte

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(GPS_REMOTE_SERVICE_UUID);

  // Convert manufacturer data to Arduino String
  String mfgDataString = "";
  for (int i = 0; i < 26; i++) {
    mfgDataString += (char)manufacturerData[i];
  }

  // Set manufacturer data using BLEAdvertisementData
  BLEAdvertisementData adData;
  adData.setManufacturerData(mfgDataString);

  // Use exact name to match official remotes (Ace Pro 2 requires exact match)
  String deviceName = "Insta360 GPS Remote";
  adData.setName(deviceName);

  pAdvertising->setAdvertisementData(adData);

  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);

  wakeMode = true;
  memcpy(currentWakePayload, wakePayload, 6);

  pAdvertising->start();
  Serial.println("Wake advertising started");
}

void setNormalAdvertising() {
  Serial.println("Setting normal advertising");

  // Stop current advertising
  BLEDevice::stopAdvertising();
  delay(100);

  // Create fresh advertising without manufacturer data
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();

  // Clear any previous advertisement data
  BLEAdvertisementData adData;

  // Use exact name to match official remotes (Ace Pro 2 requires exact match)
  String deviceName = "Insta360 GPS Remote";
  adData.setName(deviceName);
  adData.setCompleteServices(BLEUUID(GPS_REMOTE_SERVICE_UUID));

  // Set the clean advertisement data (no manufacturer data)
  pAdvertising->setAdvertisementData(adData);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);

  wakeMode = false;
  memset(currentWakePayload, 0, 6);

  pAdvertising->start();
  Serial.print("Normal advertising started with name: ");
  Serial.println(deviceName);
}

void sendCommand(uint8_t* command, size_t length, const char* commandName) {
  // Check if at least one camera is connected
  bool anyConnected = camera1Connected || camera2Connected;

  if (!anyConnected || !pServer || pServer->getConnectedCount() == 0) {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(40, 35);
    M5.Lcd.setTextColor(RED);
    M5.Lcd.println("Not Connected!");
    delay(1500);
    updateDisplay();
    return;
  }

  Serial.print("TX (Broadcast) ");
  Serial.print(commandName);
  Serial.print(": ");
  for (int i = 0; i < length; i++) {
    Serial.printf("%02X ", command[i]);
  }
  Serial.println();

  pNotifyCharacteristic->setValue(command, length);
  pNotifyCharacteristic->notify();

  // Brief visual feedback in blue bar
  showBottomStatus("SENT!", BLUE);
  delay(500);
  updateDisplay();
}

void sendUnicastCommand(uint16_t connId, uint8_t* command, size_t length, const char* commandName) {
  if (!pServer || !pNotifyCharacteristic) return;

  Serial.print("TX (Unicast ID:");
  Serial.print(connId);
  Serial.print(") ");
  Serial.print(commandName);
  Serial.print(": ");
  for (int i = 0; i < length; i++) {
    Serial.printf("%02X ", command[i]);
  }
  Serial.println();

  // Use low-level ESP API to send to specific connection
  // uint16_t gattsIf = pServer->getGattsIf(); // Private, using global capture
  uint16_t attrHandle = pNotifyCharacteristic->getHandle();
  
  // false = Notification (not Indication)
  esp_ble_gatts_send_indicate(g_gattsIf, connId, attrHandle, length, command, false);

  // Brief visual feedback in blue bar
  showBottomStatus("SYNC!", BLUE);
  delay(500);
  updateDisplay();
}

#endif // BLE_HANDLERS_H