/*
 * ui.h
 * Display and user interface functions with auto-scaling for M5StickC and M5StickC Plus2
 */

#ifndef UI_H
#define UI_H

// UI variables
int currentScreen = 0;
int pairingMenuSelection = 0; // 0=Cam1, 1=Cam2, 2=Layout, 3=Back
extern bool isVerticalLayout;

// GPIO variables
unsigned long lastPinPress[3] = {0, 0, 0}; // For G0, G26, G36
unsigned long startupTime = 0;

// External GPIO delay variable (defined in main sketch)
extern int gpioDelay;

// External recording state (defined in main sketch)
extern bool isRecording;
extern unsigned long recordingStartTime;

// Auto-detection and scaling variables
bool isPlus2 = false;
float scaleFactor = 1.0;
int baseIconSize = 32;
int scaledIconSize = 32;
int baseTextSize = 1;
int scaledTextSize = 1;

// Screen layout constants (will be scaled)
struct ScreenLayout {
  int iconX, iconY;
  int textX, textY;
  int statusX, statusY;
  int dotsY, dotsSpacing, dotsStartX;
  int instructX, instructY;
  int connectionX, connectionY, connectionRadius;
};

ScreenLayout layout;

void detectDeviceAndSetScale() {
  // Detect device by screen dimensions
  int screenWidth = M5.Lcd.width();
  int screenHeight = M5.Lcd.height();
  
  Serial.print("Screen dimensions: ");
  Serial.print(screenWidth);
  Serial.print("x");
  Serial.println(screenHeight);
  
  // Check dimensions for M5StickC Plus/Plus2 (240x135) in either orientation
  if ((screenWidth == 240 && screenHeight == 135) || (screenWidth == 135 && screenHeight == 240)) {
    // M5StickC Plus/Plus2
    isPlus2 = true;
    scaleFactor = 1.5;
    Serial.println("Detected: M5StickC Plus/Plus2");
  } else {
    // Original M5StickC or similar (160x80)
    isPlus2 = false;
    scaleFactor = 1.0;
    Serial.println("Detected: M5StickC (original)");
  }
  
  // Keep icons at original size but scale text
  scaledIconSize = baseIconSize;  // Always 32x32
  scaledTextSize = (scaleFactor >= 1.5) ? 2 : 1;
  
  // Set up layout based on device
  if (isPlus2) {
    // M5StickC Plus/Plus2 layout
    if (screenWidth > screenHeight) { // Landscape
        layout.iconX = (screenWidth - baseIconSize) / 2;
        layout.iconY = 30;
        layout.textX = screenWidth / 2;
        layout.textY = layout.iconY + baseIconSize + 10;
        layout.statusX = 220;
        layout.statusY = 12;
        layout.connectionRadius = 7;
        layout.dotsY = 120;
        layout.dotsSpacing = 25;
        layout.dotsStartX = 45;
        layout.instructX = 8;
        layout.instructY = 8;
    } else { // Portrait
        // Adapt layout for Portrait if needed for other screens
        // Currently Dashboard handles its own layout dynamiclly
        layout.dotsY = screenHeight - 15; 
        layout.dotsSpacing = 25;
        layout.dotsStartX = (screenWidth - (25 * 3)) / 2 + 12; // Center dots
    }
  } else {
    // Original M5StickC layout
    // ... (Keep existing logic, simplified for brevity if needed, but let's leave it mostly as is)
    layout.iconX = (screenWidth - baseIconSize) / 2;
    layout.iconY = 20;
    layout.textX = screenWidth / 2;
    layout.textY = layout.iconY + baseIconSize + 4;
    layout.statusX = screenWidth - 10;
    layout.statusY = 8;
    layout.connectionRadius = 5;
    layout.dotsY = screenHeight - 8;
    layout.dotsSpacing = 17;
    layout.dotsStartX = 30;
    layout.instructX = 5;
    layout.instructY = 5;
  }
  
  Serial.print("Scale factor: ");
  Serial.print(scaleFactor);
}

void applyLayoutRotation() {
    if (isVerticalLayout) {
        M5.Lcd.setRotation(0); // Portrait (Button B at bottom)
    } else {
        M5.Lcd.setRotation(3); // Landscape (Button B at right)
    }
    detectDeviceAndSetScale();
}

void drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color) {
  int16_t byteWidth = (w + 7) / 8;
  uint8_t byte = 0;

  // Draw bitmap at original size (no scaling)
  for (int16_t j = 0; j < h; j++) {
    for (int16_t i = 0; i < w; i++) {
      if (i & 7) {
        byte <<= 1;
      } else {
        byte = bitmap[j * byteWidth + i / 8];
      }
      if (byte & 0x80) {
        M5.Lcd.drawPixel(x + i, y + j, color);
      }
    }
  }
}

// Helper function to get text width for proper centering
int getTextWidth(String text, int textSize) {
  // Approximate character width based on text size
  int charWidth = (textSize == 1) ? 6 : 12;  // Size 1 = ~6px, Size 2 = ~12px per char
  return text.length() * charWidth;
}

// Helper to extract short name from full camera name
String getShortName(const char* fullName) {
  String name = String(fullName);
  if (name.length() == 0) return "NO CAM";
  
  // Remove "Insta360 " prefix if present
  if (name.startsWith("Insta360 ")) {
    name = name.substring(9);
  }
  
  // Find first space to get model name (e.g. "X3" from "X3 1234")
  int spaceIndex = name.indexOf(' ');
  if (spaceIndex > 0) {
    name = name.substring(0, spaceIndex);
  }
  
  return name;
}

// Draw a colored bar at the bottom with status message
void showBottomStatus(const char* text, uint16_t color) {
  int width = M5.Lcd.width();
  int height = M5.Lcd.height();
  
  M5.Lcd.fillRect(0, height - 25, width, 25, color);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(scaledTextSize);
  int textWidth = getTextWidth(String(text), scaledTextSize);
  M5.Lcd.setCursor((width - textWidth) / 2, height - 20);
  M5.Lcd.print(text);
}

// Clear screen and show a large centered message
void showCenteredMessage(const char* line1, const char* line2, uint16_t color) {
  M5.Lcd.fillScreen(BLACK);
  int width = M5.Lcd.width();
  int height = M5.Lcd.height();
  int centerY = height / 2;
  
  M5.Lcd.setTextColor(color);
  M5.Lcd.setTextSize(scaledTextSize); // Base size
  
  // Line 1 (Top, smaller or same?)
  if (line1 && strlen(line1) > 0) {
      int w1 = getTextWidth(String(line1), scaledTextSize);
      M5.Lcd.setCursor((width - w1) / 2, centerY - 20);
      M5.Lcd.println(line1);
  }
  
  // Line 2 (Center, larger if possible or same)
  if (line2 && strlen(line2) > 0) {
      // Make line 2 slightly larger if scale factor allows, else same
      int size2 = scaledTextSize; // Keep consistent size for readability
      M5.Lcd.setTextSize(size2);
      M5.Lcd.setTextColor(WHITE);
      int w2 = getTextWidth(String(line2), size2);
      M5.Lcd.setCursor((width - w2) / 2, centerY + 5);
      M5.Lcd.println(line2);
  }
}

void updateDashboardTimer() {
  if (currentScreen != 0) return;
  
  int width = M5.Lcd.width();
  int height = M5.Lcd.height();
  
  // Only redraw the bottom strip
  if (isRecording) {
    unsigned long duration = (millis() - recordingStartTime) / 1000;
    int minutes = duration / 60;
    int seconds = duration % 60;
    
    char timeStr[10];
    sprintf(timeStr, "%02d:%02d", minutes, seconds);
    
    // Redraw background only for the timer area
    M5.Lcd.fillRect(0, height - 25, width, 25, RED);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(scaledTextSize);
    int timeWidth = getTextWidth(String(timeStr), scaledTextSize);
    M5.Lcd.setCursor((width - timeWidth) / 2, height - 20);
    M5.Lcd.print(timeStr);
  } else {
     // Clear the timer area if we stopped recording but didn't do a full refresh yet
     // (Though usually a full updateDisplay is called on stop)
     M5.Lcd.fillRect(0, height - 25, width, 25, BLACK);
  }
}

void drawDashboard() {
  int width = M5.Lcd.width();
  int height = M5.Lcd.height();
  int halfWidth = width / 2;
  
  // Remote Battery Indicator (Top Right)
  int batLevel = M5.Power.getBatteryLevel();
  M5.Lcd.setTextSize(1);
  if (batLevel > 20) M5.Lcd.setTextColor(GREEN);
  else M5.Lcd.setTextColor(RED);
  M5.Lcd.setCursor(width - 25, 5);
  M5.Lcd.print(batLevel);
  M5.Lcd.print("%");

  // --- Camera 1 Setup ---
  uint16_t c1Color = DARKGREY;
  String c1Name = "EMPTY";
  if (camera1.isValid) {
    c1Name = getShortName(camera1.name);
    if (camera1Connected) c1Color = BLUE;
    else c1Color = RED;
  }

  // --- Camera 2 Setup ---
  uint16_t c2Color = DARKGREY;
  String c2Name = "EMPTY";
  if (camera2.isValid) {
    c2Name = getShortName(camera2.name);
    if (camera2Connected) c2Color = BLUE;
    else c2Color = RED;
  }

  if (!isVerticalLayout) {
      // --- HORIZONTAL LAYOUT (Side by Side) ---
      // Draw divider
      M5.Lcd.drawLine(halfWidth, 10, halfWidth, height - 20, DARKGREY);
      
      // Cam 1 (Left)
      int c1X = halfWidth / 2;
      int c1Y = height / 2 - 10;
      M5.Lcd.fillCircle(c1X, c1Y - 15, isPlus2 ? 15 : 10, c1Color);
      
      M5.Lcd.setTextSize(scaledTextSize);
      M5.Lcd.setTextColor(WHITE);
      int name1Width = getTextWidth(c1Name, scaledTextSize);
      int text1X = c1X - (name1Width/2);
      int text1Y = c1Y + 10; // Increased spacing (+5)
      M5.Lcd.setCursor(text1X, text1Y);
      M5.Lcd.print(c1Name);

      // Recording Dot 1 (Relative to Text Top-Right)
      if (camera1Connected && camera1.isRecording) {
          // Position: Right of text + 6px, Top of text + 2px
          M5.Lcd.fillCircle(text1X + name1Width + 6, text1Y + 2, 5, RED);
      }
      
      // Cam 2 (Right)
      int c2X = halfWidth + (halfWidth / 2);
      int c2Y = height / 2 - 10;
      M5.Lcd.fillCircle(c2X, c2Y - 15, isPlus2 ? 15 : 10, c2Color);
      
      M5.Lcd.setTextSize(scaledTextSize);
      M5.Lcd.setTextColor(WHITE);
      int name2Width = getTextWidth(c2Name, scaledTextSize);
      int text2X = c2X - (name2Width/2);
      int text2Y = c2Y + 10; // Increased spacing (+5)
      M5.Lcd.setCursor(text2X, text2Y);
      M5.Lcd.print(c2Name);

      // Recording Dot 2
      if (camera2Connected && camera2.isRecording) {
          M5.Lcd.fillCircle(text2X + name2Width + 6, text2Y + 2, 5, RED);
      }
      
  } else {
      // --- VERTICAL LAYOUT (Top / Bottom) ---
      int halfHeight = height / 2;
      // Draw divider
      M5.Lcd.drawLine(10, halfHeight, width - 10, halfHeight, DARKGREY);
      
      // Cam 1 (Top)
      int c1X = width / 2;
      int c1Y = halfHeight / 2; 
      M5.Lcd.fillCircle(c1X, c1Y - 10, isPlus2 ? 15 : 10, c1Color);
      
      M5.Lcd.setTextSize(scaledTextSize);
      M5.Lcd.setTextColor(WHITE);
      int name1Width = getTextWidth(c1Name, scaledTextSize);
      int text1X = c1X - (name1Width/2);
      int text1Y = c1Y + 15; // Increased spacing (+5 from previous +10)
      M5.Lcd.setCursor(text1X, text1Y);
      M5.Lcd.print(c1Name);

      // Recording Dot 1
      if (camera1Connected && camera1.isRecording) {
          M5.Lcd.fillCircle(text1X + name1Width + 6, text1Y + 2, 5, RED);
      }
      
      // Cam 2 (Bottom)
      int c2X = width / 2;
      int c2Y = halfHeight + (halfHeight / 2) - 10;
      M5.Lcd.fillCircle(c2X, c2Y - 10, isPlus2 ? 15 : 10, c2Color);
      
      M5.Lcd.setTextSize(scaledTextSize);
      M5.Lcd.setTextColor(WHITE);
      int name2Width = getTextWidth(c2Name, scaledTextSize);
      int text2X = c2X - (name2Width/2);
      int text2Y = c2Y + 15; // Increased spacing (+5)
      M5.Lcd.setCursor(text2X, text2Y);
      M5.Lcd.print(c2Name);

      // Recording Dot 2
      if (camera2Connected && camera2.isRecording) {
          M5.Lcd.fillCircle(text2X + name2Width + 6, text2Y + 2, 5, RED);
      }
  }
  
  // --- Recording Status (Bottom) ---
  if (isRecording) {
    updateDashboardTimer();
  }
}

// External pairing menu selection (defined at top of this file)

void drawPairingMenu() {
  int width = M5.Lcd.width();
  int height = M5.Lcd.height();
  int numItems = 4;
  int itemHeight = height / numItems;
  
  // Force smaller text in Vertical mode to fit width
  int menuTextSize = (isVerticalLayout) ? 1 : scaledTextSize;
  
  String layoutStr = isVerticalLayout ? "LAYOUT: VERT" : "LAYOUT: HORIZ";
  String items[] = {"PAIR SLOT 1", "PAIR SLOT 2", layoutStr, "BACK"};
  uint16_t colors[] = {ICON_BLUE, ICON_CYAN, ICON_YELLOW, WHITE};
  
  for (int i = 0; i < numItems; i++) {
    int y = i * itemHeight;
    
    // Highlight selection
    if (i == pairingMenuSelection) {
      M5.Lcd.fillRect(0, y, width, itemHeight, DARKGREY);
      M5.Lcd.drawRect(0, y, width, itemHeight, WHITE);
    }
    
    // Draw Text
    M5.Lcd.setTextColor(colors[i]);
    M5.Lcd.setTextSize(menuTextSize);
    int textWidth = getTextWidth(items[i], menuTextSize);
    M5.Lcd.setCursor((width - textWidth) / 2, y + (itemHeight/2) - 5);
    M5.Lcd.print(items[i]);
  }
}

void updateDisplay() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(scaledTextSize);
  
  if (currentScreen == 0) {
    drawDashboard();
  } else if (currentScreen == 1) {
    drawPairingMenu();
  }
}

void showNotConnectedMessage() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(scaledTextSize);
  int msgX = isPlus2 ? 40 : 30;
  int msgY = isPlus2 ? 55 : 35;
  M5.Lcd.setCursor(msgX, msgY);
  M5.Lcd.setTextColor(RED);
  M5.Lcd.println("Not Connected!");
  delay(1500);
  updateDisplay();
}

void showNoCameraMessage() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(scaledTextSize);
  int msgX = isPlus2 ? 25 : 25;
  int msgY1 = isPlus2 ? 45 : 30;
  int msgY2 = isPlus2 ? 70 : 45;
  
  M5.Lcd.setCursor(msgX, msgY1);
  M5.Lcd.setTextColor(RED);
  M5.Lcd.println("No camera paired!");
  M5.Lcd.setCursor(msgX + (isPlus2 ? 10 : 5), msgY2);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.println("Connect first");
  delay(2000);
  updateDisplay();
}

void checkGPIOPins() {
  unsigned long currentTime = millis();
  
  // Skip GPIO checks during startup delay
  if (currentTime - startupTime < startupDelay) {
    return; // GPIO input disabled during startup
  }
  
  // One-time message when GPIO becomes active
  static bool gpioActivationMessageShown = false;
  if (!gpioActivationMessageShown) {
    Serial.println("GPIO input now active!");
    gpioActivationMessageShown = true;
  }
  
  // Check G0 (SHUTTER_PIN) - Function #2 - Trigger on LOW (to GND)
  static bool lastShutterState = HIGH; // G0 defaults to HIGH due to hardware pullup
  bool currentShutterState = digitalRead(SHUTTER_PIN);
  
  if (currentShutterState == LOW && lastShutterState == HIGH && (currentTime - lastPinPress[0] > debounceDelay)) {
    lastPinPress[0] = currentTime;
    Serial.print("GPIO Pin G0 activated (pulled to GND) - Delaying ");
    Serial.print(gpioDelay);
    Serial.println("ms then executing Shutter");
    delay(gpioDelay);  // Apply unique delay before executing
    executeShutter();
    // Also toggle timer for GPIO press
    if (!isRecording) {
        isRecording = true;
        recordingStartTime = millis();
    } else {
        isRecording = false;
    }
    updateDisplay();
  }
  lastShutterState = currentShutterState;
  
  // Check G26 (SLEEP_PIN) - Function #5 - Trigger on HIGH (to 3.3V)
  static bool lastSleepState = LOW;
  bool currentSleepState = digitalRead(SLEEP_PIN);
  
  if (currentSleepState == HIGH && lastSleepState == LOW && (currentTime - lastPinPress[1] > debounceDelay)) {
    lastPinPress[1] = currentTime;
    Serial.print("GPIO Pin G26 activated - Delaying ");
    Serial.print(gpioDelay);
    Serial.println("ms then executing Sleep");
    delay(gpioDelay);  // Apply unique delay before executing
    executeSleep();
  }
  lastSleepState = currentSleepState;
  
  // Check G36 (WAKE_PIN) - Function #6 - Trigger on HIGH (to 3.3V)
  static bool lastWakeState = LOW;
  bool currentWakeState = digitalRead(WAKE_PIN);
  
  if (currentWakeState == HIGH && lastWakeState == LOW && (currentTime - lastPinPress[2] > debounceDelay)) {
    lastPinPress[2] = currentTime;
    Serial.print("GPIO Pin G36 activated - Delaying ");
    Serial.print(gpioDelay);
    Serial.println("ms then executing Wake");
    delay(gpioDelay);  // Apply unique delay before executing
    executeWake();
  }
  lastWakeState = currentWakeState;
}

#endif // UI_H