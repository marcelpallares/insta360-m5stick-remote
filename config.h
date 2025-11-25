/*
 * config.h
 * Configuration constants, pin definitions, and UUIDs
 */

#ifndef CONFIG_H
#define CONFIG_H

// GPIO pin definitions for external button control
#define SHUTTER_PIN G0   // Pin for Shutter function (#2) - triggers on LOW (to GND)
#define SLEEP_PIN G26    // Pin for Sleep function (#5) - triggers on HIGH (to 3.3V)
#define WAKE_PIN G36     // Pin for Wake function (#6) - triggers on HIGH (to 3.3V)

// GPS Remote service UUIDs
#define GPS_REMOTE_SERVICE_UUID      "0000ce80-0000-1000-8000-00805f9b34fb"
#define GPS_REMOTE_WRITE_CHAR_UUID   "0000ce81-0000-1000-8000-00805f9b34fb"
#define GPS_REMOTE_NOTIFY_CHAR_UUID  "0000ce82-0000-1000-8000-00805f9b34fb"

// Screen colors
#define ICON_BLUE 0x001F
#define ICON_RED 0xF800
#define ICON_ORANGE 0xFC00
#define ICON_PINK 0xF81F
#define ICON_PURPLE 0x8010
#define ICON_YELLOW 0xFFE0
#define ICON_CYAN 0x07FF
#define ICON_WHITE 0xFFFF

// UI constants
const int numScreens = 7;

// GPIO debounce settings
const unsigned long debounceDelay = 200; // 200ms debounce
const unsigned long startupDelay = 2000; // 2 seconds delay after startup

// Command payloads for camera control
static uint8_t SHUTTER_CMD[] = {0xFC, 0xEF, 0xFE, 0x86, 0x00, 0x03, 0x01, 0x02, 0x00};
static uint8_t MODE_CMD[] = {0xFC, 0xEF, 0xFE, 0x86, 0x00, 0x03, 0x01, 0x01, 0x00};
static uint8_t TOGGLE_SCREEN_CMD[] = {0xFC, 0xEF, 0xFE, 0x86, 0x00, 0x03, 0x01, 0x00, 0x00};
static uint8_t POWER_OFF_CMD[] = {0xFC, 0xEF, 0xFE, 0x86, 0x00, 0x03, 0x01, 0x00, 0x03};

#endif // CONFIG_H
