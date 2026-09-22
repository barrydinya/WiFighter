/*
 * WiFighter - M5StickC Plus / Plus2
 * Dual BLE + WiFi persistence tracker
 * Detects devices that linger or disappear from the RF environment.
 *
 * ETHICAL USE ONLY - authorized security research / personal privacy monitoring.
 * Unauthorized tracking or disruption is illegal.
 *
 * Hardware: M5StickC Plus or Plus2
 * Board: M5Stick-C-Plus or M5StickC Plus2 (Arduino IDE / PlatformIO)
 * Libraries: M5Unified, NimBLE-Arduino, WiFi
 *
 * Controls (typical M5StickC Plus):
 *   BtnA (front)  : Select / Enter / Toggle scan on Home
 *   BtnB (side)   : Next / Back / Open Menu
 *   Power button  : Long press power off (Plus2 behavior may differ)
 */

#include <M5Unified.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <NimBLEDevice.h>   // Prefer NimBLE over classic BLE for memory + speed
#include <Preferences.h>
#include <vector>
#include <algorithm>
#include "oui.h"

// ---------------------------------------------------------------------------
// CONFIG
// ---------------------------------------------------------------------------
#define MAX_DEVICES          64
#define SCAN_INTERVAL_MS     4000
#define HOME_REFRESH_MS      800
#define MENU_TIMEOUT_MS      15000
#define PERSIST_SCORE_MIN    0.45f   // below this = transient
#define LEFT_THRESHOLD_MS    45000   // not seen for this long = "left"

// ---------------------------------------------------------------------------
// DEVICE RECORD
// ---------------------------------------------------------------------------
struct DeviceRec {
  uint8_t  mac[6];
  char     name[24];
  char     vendor[16];
  int8_t   rssi;
  uint32_t firstSeen;
  uint32_t lastSeen;
  uint16_t hitCount;
  float    persistScore;
  bool     isBLE;
  bool     isWiFi;
  bool     flaggedLeft;
  bool     isTracker;          // AirTag / Tile / etc heuristic
};

std::vector<DeviceRec> devices;
Preferences prefs;

// ---------------------------------------------------------------------------
// UI STATE
// ---------------------------------------------------------------------------
enum Screen : uint8_t {
  SCREEN_HOME = 0,
  SCREEN_MENU,
  SCREEN_SCAN,
  SCREEN_DEVICES,
  SCREEN_ALERTS,
  SCREEN_SETTINGS,
  SCREEN_ABOUT
};

Screen currentScreen = SCREEN_HOME;
int    menuIndex     = 0;
int    deviceScroll  = 0;
bool   scanning      = false;
uint32_t lastScan    = 0;
uint32_t lastHomeDraw = 0;
uint32_t menuEntered = 0;

const char* menuItems[] = {
  "Start Scan",
  "Device List",
  "Alerts / Left",
  "Settings",
  "About",
  "Back to Home"
};
const int MENU_COUNT = 6;

// ---------------------------------------------------------------------------
// HELPERS
// ---------------------------------------------------------------------------
void macToStr(const uint8_t* mac, char* buf) {
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

bool sameMac(const uint8_t* a, const uint8_t* b) {
  return memcmp(a, b, 6) == 0;
}

DeviceRec* findDevice(const uint8_t* mac) {
  for (auto& d : devices) {
    if (sameMac(d.mac, mac)) return &d;
  }
  return nullptr;
}

const char* lookupVendor(const uint8_t* mac) {
  char prefix[9];
  sprintf(prefix, "%02X:%02X:%02X", mac[0], mac[1], mac[2]);
  for (int i = 0; i < OUI_COUNT; i++) {
    if (strcasecmp(prefix, ouiTable[i].prefix) == 0) {
      return ouiTable[i].name;
    }
  }
  return "Unknown";
}

float calcPersistScore(const DeviceRec& d) {
  // Simple multi-factor: hits + continuity + recency
  uint32_t age = millis() - d.firstSeen;
  if (age < 5000) return 0.1f;
  float hitsFactor = constrain((float)d.hitCount / 12.0f, 0.0f, 1.0f);
  float contFactor = 1.0f - constrain((float)(millis() - d.lastSeen) / 60000.0f, 0.0f, 1.0f);
  float ageFactor  = constrain((float)age / 180000.0f, 0.0f, 1.0f); // up to 3 min
  return (hitsFactor * 0.45f) + (contFactor * 0.35f) + (ageFactor * 0.20f);
}

// ---------------------------------------------------------------------------
// DRAWING PRIMITIVES (M5.Display = M5GFX)
// ---------------------------------------------------------------------------
void clearScreen() {
  M5.Display.fillScreen(TFT_BLACK);
}

void drawHeader(const char* title) {
  M5.Display.fillRect(0, 0, M5.Display.width(), 18, TFT_NAVY);
  M5.Display.setTextColor(TFT_WHITE, TFT_NAVY);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(4, 5);
  M5.Display.print(title);
  // battery rough %
  int bat = M5.Power.getBatteryLevel();
  M5.Display.setCursor(M5.Display.width() - 30, 5);
  M5.Display.printf("%d%%", bat);
}

void drawFooter(const char* left, const char* right) {
  int y = M5.Display.height() - 14;
  M5.Display.fillRect(0, y, M5.Display.width(), 14, TFT_DARKGREY);
  M5.Display.setTextColor(TFT_WHITE, TFT_DARKGREY);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(4, y + 3);
  M5.Display.print(left);
  M5.Display.setCursor(M5.Display.width() - 50, y + 3);
  M5.Display.print(right);
}

// ---------------------------------------------------------------------------
// HOME SCREEN
// ---------------------------------------------------------------------------
void drawHome() {
  clearScreen();
  drawHeader("WiFighter");

  int cx = M5.Display.width() / 2;
  int cy = 42;

  // Status circle
  uint16_t col = scanning ? TFT_GREEN : TFT_ORANGE;
  M5.Display.fillCircle(cx, cy, 18, col);
  M5.Display.drawCircle(cx, cy, 20, TFT_WHITE);

  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(cx - 18, cy + 26);
  M5.Display.print(scanning ? "SCANNING" : "IDLE");

  // Stats
  int active = 0, left = 0, high = 0;
  for (auto& d : devices) {
    if (millis() - d.lastSeen < LEFT_THRESHOLD_MS) active++;
    else { d.flaggedLeft = true; left++; }
    if (d.persistScore >= PERSIST_SCORE_MIN) high++;
  }

  M5.Display.setCursor(8, 95);
  M5.Display.printf("Devices : %d", devices.size());
  M5.Display.setCursor(8, 108);
  M5.Display.printf("Active  : %d", active);
  M5.Display.setCursor(8, 121);
  M5.Display.printf("Left    : %d", left);
  M5.Display.setCursor(8, 134);
  M5.Display.printf("Persist : %d", high);

  drawFooter("Menu:B", "A:Select");
}

// ---------------------------------------------------------------------------
// MENU SCREEN
// ---------------------------------------------------------------------------
void drawMenu() {
  clearScreen();
  drawHeader("MENU");

  for (int i = 0; i < MENU_COUNT; i++) {
    int y = 24 + i * 18;
    if (i == menuIndex) {
      M5.Display.fillRect(0, y - 2, M5.Display.width(), 16, TFT_DARKCYAN);
      M5.Display.setTextColor(TFT_WHITE, TFT_DARKCYAN);
    } else {
      M5.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    }
    M5.Display.setCursor(10, y);
    M5.Display.print(menuItems[i]);
  }
  drawFooter("Next:B", "Enter:A");
}

// ---------------------------------------------------------------------------
// DEVICE LIST (scrollable)
// ---------------------------------------------------------------------------
void drawDevices() {
  clearScreen();
  drawHeader("DEVICES");

  if (devices.empty()) {
    M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5.Display.setCursor(20, 60);
    M5.Display.print("No devices yet");
    M5.Display.setCursor(10, 80);
    M5.Display.print("Start a scan first");
  } else {
    int maxVis = 6;
    int start = deviceScroll;
    for (int i = 0; i < maxVis && (start + i) < (int)devices.size(); i++) {
      auto& d = devices[start + i];
      char mac[18];
      macToStr(d.mac, mac);
      int y = 22 + i * 18;
      uint16_t col = d.flaggedLeft ? TFT_RED :
                     (d.persistScore >= PERSIST_SCORE_MIN ? TFT_ORANGE : TFT_GREEN);
      M5.Display.setTextColor(col, TFT_BLACK);
      M5.Display.setCursor(2, y);
      M5.Display.printf("%s %ddB", mac + 9, d.rssi); // short MAC
      M5.Display.setCursor(90, y);
      M5.Display.printf("%.2f", d.persistScore);
    }
  }
  drawFooter("Scroll:B", "Back:A");
}

// ---------------------------------------------------------------------------
// ALERTS / LEFT DEVICES
// ---------------------------------------------------------------------------
void drawAlerts() {
  clearScreen();
  drawHeader("LEFT / ALERTS");

  int shown = 0;
  for (auto& d : devices) {
    if (!d.flaggedLeft && d.persistScore < PERSIST_SCORE_MIN) continue;
    if (shown >= 6) break;
    char mac[18];
    macToStr(d.mac, mac);
    int y = 22 + shown * 18;
    M5.Display.setTextColor(d.flaggedLeft ? TFT_RED : TFT_ORANGE, TFT_BLACK);
    M5.Display.setCursor(2, y);
    M5.Display.printf("%s %s", mac + 9, d.flaggedLeft ? "LEFT" : "HIGH");
    shown++;
  }
  if (shown == 0) {
    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
    M5.Display.setCursor(30, 60);
    M5.Display.print("All clear");
  }
  drawFooter("Refresh:B", "Back:A");
}

// ---------------------------------------------------------------------------
// SETTINGS + ABOUT (stubs for now)
// ---------------------------------------------------------------------------
void drawSettings() {
  clearScreen();
  drawHeader("SETTINGS");
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setCursor(8, 30);
  M5.Display.print("Scan interval: 4s");
  M5.Display.setCursor(8, 48);
  M5.Display.print("Left thresh: 45s");
  M5.Display.setCursor(8, 66);
  M5.Display.print("Persist min: 0.45");
  M5.Display.setCursor(8, 90);
  M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
  M5.Display.print("(edit in source)");
  drawFooter("", "Back:A");
}

void drawAbout() {
  clearScreen();
  drawHeader("ABOUT");
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.setCursor(8, 30);
  M5.Display.print("WiFighter v0.1");
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setCursor(8, 50);
  M5.Display.print("BLE + WiFi tracker");
  M5.Display.setCursor(8, 68);
  M5.Display.print("Detects linger/left");
  M5.Display.setCursor(8, 90);
  M5.Display.print("M5StickC Plus/Plus2");
  M5.Display.setCursor(8, 110);
  M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Display.print("Ethical use only");
  drawFooter("", "Back:A");
}

// ---------------------------------------------------------------------------
// SCAN LOGIC (skeleton – expand next)
// ---------------------------------------------------------------------------
void startScan() {
  scanning = true;
  lastScan = millis();
  // WiFi scan (async style)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  // BLE scan would go here with NimBLE
  // For now we just mark state; real scan implementation next iteration
}

void stopScan() {
  scanning = false;
}

void processScanResults() {
  // Placeholder – real WiFi + BLE result ingestion will fill devices vector
  // and update persistScore / flaggedLeft
  for (auto& d : devices) {
    d.persistScore = calcPersistScore(d);
    if (millis() - d.lastSeen > LEFT_THRESHOLD_MS) {
      d.flaggedLeft = true;
    }
  }
}

// ---------------------------------------------------------------------------
// BUTTON HANDLING
// ---------------------------------------------------------------------------
void handleButtons() {
  M5.update();

  bool aPressed = M5.BtnA.wasPressed();
  bool bPressed = M5.BtnB.wasPressed();

  switch (currentScreen) {
    case SCREEN_HOME:
      if (bPressed) {
        currentScreen = SCREEN_MENU;
        menuIndex = 0;
        menuEntered = millis();
        drawMenu();
      }
      if (aPressed) {
        // quick toggle scan from home
        if (scanning) stopScan();
        else startScan();
        drawHome();
      }
      break;

    case SCREEN_MENU:
      if (bPressed) {
        menuIndex = (menuIndex + 1) % MENU_COUNT;
        drawMenu();
      }
      if (aPressed) {
        switch (menuIndex) {
          case 0: // Start Scan
            startScan();
            currentScreen = SCREEN_HOME;
            drawHome();
            break;
          case 1:
            currentScreen = SCREEN_DEVICES;
            deviceScroll = 0;
            drawDevices();
            break;
          case 2:
            currentScreen = SCREEN_ALERTS;
            drawAlerts();
            break;
          case 3:
            currentScreen = SCREEN_SETTINGS;
            drawSettings();
            break;
          case 4:
            currentScreen = SCREEN_ABOUT;
            drawAbout();
            break;
          case 5:
            currentScreen = SCREEN_HOME;
            drawHome();
            break;
        }
      }
      // auto return after timeout
      if (millis() - menuEntered > MENU_TIMEOUT_MS) {
        currentScreen = SCREEN_HOME;
        drawHome();
      }
      break;

    case SCREEN_DEVICES:
      if (bPressed) {
        if (!devices.empty()) {
          deviceScroll = (deviceScroll + 1) % devices.size();
          drawDevices();
        }
      }
      if (aPressed) {
        currentScreen = SCREEN_MENU;
        drawMenu();
      }
      break;

    case SCREEN_ALERTS:
      if (bPressed) drawAlerts(); // refresh
      if (aPressed) {
        currentScreen = SCREEN_MENU;
        drawMenu();
      }
      break;

    case SCREEN_SETTINGS:
    case SCREEN_ABOUT:
      if (aPressed) {
        currentScreen = SCREEN_MENU;
        drawMenu();
      }
      break;

    default:
      break;
  }
}

// ---------------------------------------------------------------------------
// SETUP / LOOP
// ---------------------------------------------------------------------------
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(1);          // landscape for Plus
  M5.Display.setBrightness(80);
  M5.Display.setTextSize(1);

  prefs.begin("wifighter", false);

  // Optional: restore last known devices later
  devices.reserve(MAX_DEVICES);

  clearScreen();
  drawHome();
}

void loop() {
  handleButtons();

  // periodic home refresh while on home
  if (currentScreen == SCREEN_HOME && millis() - lastHomeDraw > HOME_REFRESH_MS) {
    processScanResults();
    drawHome();
    lastHomeDraw = millis();
  }

  // keep scan alive
  if (scanning && millis() - lastScan > SCAN_INTERVAL_MS) {
    // trigger next scan cycle here
    lastScan = millis();
    processScanResults();
  }

  delay(20); // light yield
}
