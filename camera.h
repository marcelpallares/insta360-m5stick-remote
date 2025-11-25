/*
 * camera.h
 * Camera structure and management functions
 */

#ifndef CAMERA_H
#define CAMERA_H

// Camera info structure
struct CameraInfo {
  char name[30];
  char address[20];
  uint8_t wakePayload[6];
  bool isValid;
  uint16_t connId;
  int batteryLevel;
  bool isRecording;
  unsigned long lastTimerTime;
};

// Global camera variables - dual camera support
CameraInfo camera1;
CameraInfo camera2;
Preferences preferences;

// UI Settings (Global)
bool isVerticalLayout = false;

// Pairing mode variables
bool pairingMode = false;
int pairingCameraSlot = 0;  // 1 for camera1, 2 for camera2
String detectedCameraName = "";
String detectedCameraAddress = "";

// Connection tracking for both cameras
bool camera1Connected = false;
bool camera2Connected = false;
String camera1ConnectedAddress = "";
String camera2ConnectedAddress = "";

// Wake-up variables
bool wakeMode = false;
uint8_t currentWakePayload[6] = {0};

void saveLayoutPreference(bool vertical) {
  preferences.begin("ui_settings", false);
  preferences.putBool("vert_layout", vertical);
  preferences.end();
  isVerticalLayout = vertical;
  Serial.print("Layout saved: ");
  Serial.println(vertical ? "Vertical" : "Horizontal");
}

void loadCamera(int cameraNum, CameraInfo* camera) {
  String namespaceName = (cameraNum == 1) ? "camera1" : "camera2";
  Serial.print("Loading camera ");
  Serial.print(cameraNum);
  Serial.println(" from preferences...");

  preferences.begin(namespaceName.c_str(), false);

  preferences.getString("name", camera->name, 30);
  preferences.getString("address", camera->address, 20);
  
  // Trim any potential whitespace from address
  String addrStr = String(camera->address);
  addrStr.trim();
  addrStr.toCharArray(camera->address, 20);

  Serial.print("Loaded name: ");
  Serial.println(camera->name);
  Serial.print("Loaded address: ");
  Serial.println(camera->address);

  size_t len = preferences.getBytesLength("wake");

  if (len == 6) {
    preferences.getBytes("wake", camera->wakePayload, 6);
    camera->isValid = (strlen(camera->name) > 0);
    camera->connId = 0xFFFF;
    camera->batteryLevel = -1;
    camera->isRecording = false;
    camera->lastTimerTime = 0;

    if (camera->isValid) {
      Serial.print("Wake payload loaded: ");
      for (int i = 0; i < 6; i++) {
        Serial.printf("%02X ", camera->wakePayload[i]);
      }
      Serial.println();
    }
  } else {
    camera->isValid = false;
    Serial.println("No valid wake payload found");
  }

  preferences.end();

  Serial.print("Camera ");
  Serial.print(cameraNum);
  Serial.print(" isValid: ");
  Serial.println(camera->isValid);
}

void loadAllCameras() {
  Serial.println("=== Loading all cameras ===");
  loadCamera(1, &camera1);
  loadCamera(2, &camera2);
  
  // Load UI Settings
  preferences.begin("ui_settings", false);
  isVerticalLayout = preferences.getBool("vert_layout", false);
  preferences.end();
  Serial.print("Loaded Layout: ");
  Serial.println(isVerticalLayout ? "Vertical" : "Horizontal");
  
  Serial.println("=== Camera loading complete ===");
}

void saveCamera(int cameraNum, String cameraName, String cameraAddress) {
  CameraInfo* camera = (cameraNum == 1) ? &camera1 : &camera2;
  String namespaceName = (cameraNum == 1) ? "camera1" : "camera2";

  Serial.print("Saving camera ");
  Serial.print(cameraNum);
  Serial.print(": ");
  Serial.print(cameraName);
  Serial.print(" @ ");
  Serial.println(cameraAddress);

  // Extract wake payload from camera name (last 6 characters)
  if (cameraName.length() >= 6) {
    String nameEnd = cameraName.substring(cameraName.length() - 6);
    Serial.print("Wake payload suffix: ");
    Serial.println(nameEnd);

    // Convert to ASCII bytes
    for (int i = 0; i < 6; i++) {
      camera->wakePayload[i] = (uint8_t)nameEnd[i];
    }

    // Save camera info
    snprintf(camera->name, 30, "%s", cameraName.c_str());
    snprintf(camera->address, 20, "%s", cameraAddress.c_str());
    camera->isValid = true;
    camera->connId = 0xFFFF;
    camera->batteryLevel = -1;
    camera->isRecording = false;
    camera->lastTimerTime = 0;

    // Store in preferences
    preferences.begin(namespaceName.c_str(), false);
    preferences.putString("name", camera->name);
    preferences.putString("address", camera->address);
    preferences.putBytes("wake", camera->wakePayload, 6);
    preferences.end();

    Serial.print("Wake payload bytes: ");
    for (int i = 0; i < 6; i++) {
      Serial.printf("%02X ", camera->wakePayload[i]);
    }
    Serial.println();
    Serial.print("Camera ");
    Serial.print(cameraNum);
    Serial.println(" saved successfully");
  } else {
    Serial.println("Camera name too short for valid wake payload");
    camera->isValid = false;
  }
}

#endif // CAMERA_H