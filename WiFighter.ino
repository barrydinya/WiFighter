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
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <vector>
#include <algorithm>
#include "oui.h"

// ---------------------------------------------------------------------------
// CONFIG
// ---------------------------------------------------------------------------
#define MAX_DEVICES          64
#define SCAN_INTERVAL_MS     5000
#define HOME_REFRESH_MS      1000
#define MENU_TIMEOUT_MS      20000
#define PERSIST_SCORE_MIN    0.45f
#define LEFT_THRESHOLD_MS    45000
#define BLE_SCAN_SECONDS     3

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
  bool     isTracker;
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
bool   bleScanning   = false;
uint32_t lastScan    = 0;
uint32_t lastHomeDraw = 0;
uint32_t menuEntered = 0;
uint32_t lastBleStart = 0;

const char* menuItems[] = {
  "Start / Stop Scan",
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
  uint32_t age = millis() - d.firstSeen;
  if (age < 5000) return 0.1f;
  float hitsFactor = constrain((float)d.hitCount / 12.0f, 0.0f, 1.0f);
  float contFactor = 1.0f - constrain((float)(millis() - d.lastSeen) / 60000.0f, 0.0f, 1.0f);
  float ageFactor  = constrain((float)age / 180000.0f, 0.0f, 1.0f);
  return (hitsFactor * 0.45f) + (contFactor * 0.35f) + (ageFactor * 0.20f);
}

void parseMacString(const String& s, uint8_t* out) {
  unsigned int b[6];
  if (sscanf(s.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x",
             &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) == 6) {
    for (int i = 0; i < 6; i++) out[i] = (uint8_t)b[i];
  } else {
    memset(out, 0, 6);
  }
}

// ---------------------------------------------------------------------------
// DRAWING
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
  M5.Display.setCursor(M5.Display.width() - 55, y + 3);
  M5.Display.print(right);
}

// ---------------------------------------------------------------------------
// HOME SCREEN
// ---------------------------------------------------------------------------
void drawHome() {
  clearScreen();
  drawHeader("WiFighter");

  int cx = M5.Display.width() / 2;
  int cy = 38;

  uint16_t col = scanning ? TFT_GREEN : TFT_ORANGE;
  M5.Display.fillCircle(cx, cy, 16, col);
  M5.Display.drawCircle(cx, cy, 18, TFT_WHITE);

  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(cx - 22, cy + 22);
  M5.Display.print(scanning ? "SCANNING" : "  IDLE  ");

  int active = 0, left = 0, high = 0;
  for (auto& d : devices) {
    if (millis() - d.lastSeen < LEFT_THRESHOLD_MS) {
      active++;
      d.flaggedLeft = false;
    } else {
      d.flaggedLeft = true;
      left++;
    }
    if (d.persistScore >= PERSIST_SCORE_MIN) high++;
  }

  int y = 78;
  M5.Display.setCursor(8, y); M5.Display.printf("Total   : %d", (int)devices.size()); y += 13;
  M5.Display.setCursor(8, y); M5.Display.printf("Active  : %d", active); y += 13;
  M5.Display.setCursor(8, y); M5.Display.printf("Left    : %d", left); y += 13;
  M5.Display.setCursor(8, y); M5.Display.printf("Persist : %d", high); y += 16;

  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.setCursor(8, y);
  M5.Display.print(scanning ? "A: Stop   B: Menu" : "A: Start  B: Menu");

  drawFooter("Menu", "Toggle");
}

// ---------------------------------------------------------------------------
// MENU
// ---------------------------------------------------------------------------
void drawMenu() {
  clearScreen();
  drawHeader("MENU");

  for (int i = 0; i < MENU_COUNT; i++) {
    int y = 22 + i * 17;
    if (i == menuIndex) {
      M5.Display.fillRect(0, y - 2, M5.Display.width(), 15, TFT_DARKCYAN);
      M5.Display.setTextColor(TFT_WHITE, TFT_DARKCYAN);
    } else {
      M5.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    }
    M5.Display.setCursor(8, y);
    M5.Display.print(menuItems[i]);
  }
  drawFooter("Next", "Enter");
}

// ---------------------------------------------------------------------------
// DEVICE LIST
// ---------------------------------------------------------------------------
void drawDevices() {
  clearScreen();
  drawHeader("DEVICES");

  if (devices.empty()) {
    M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5.Display.setCursor(18, 55);
    M5.Display.print("No devices yet");
    M5.Display.setCursor(10, 75);
    M5.Display.print("Start a scan first");
  } else {
    int maxVis = 6;
    int start = deviceScroll;
    if (start >= (int)devices.size()) start = 0;
    for (int i = 0; i < maxVis && (start + i) < (int)devices.size(); i++) {
      auto& d = devices[start + i];
      char mac[18];
      macToStr(d.mac, mac);
      int y = 20 + i * 17;
      uint16_t col = d.flaggedLeft ? TFT_RED :
                     (d.persistScore >= PERSIST_SCORE_MIN ? TFT_ORANGE : TFT_GREEN);
      M5.Display.setTextColor(col, TFT_BLACK);
      M5.Display.setCursor(2, y);
      M5.Display.printf("%s %d%s", mac + 9, d.rssi, d.isBLE ? "B" : "W");
      M5.Display.setCursor(95, y);
      M5.Display.printf("%.2f", d.persistScore);
    }
  }
  drawFooter("Scroll", "Back");
}

// ---------------------------------------------------------------------------
// ALERTS / LEFT
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
    int y = 20 + shown * 17;
    M5.Display.setTextColor(d.flaggedLeft ? TFT_RED : TFT_ORANGE, TFT_BLACK);
    M5.Display.setCursor(2, y);
    M5.Display.printf("%s %s", mac + 9, d.flaggedLeft ? "LEFT" : "HIGH");
    shown++;
  }
  if (shown == 0) {
    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
    M5.Display.setCursor(28, 55);
    M5.Display.print("All clear");
  }
  drawFooter("Refresh", "Back");
}

// ---------------------------------------------------------------------------
// SETTINGS + ABOUT
// ---------------------------------------------------------------------------
void drawSettings() {
  clearScreen();
  drawHeader("SETTINGS");
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setCursor(8, 28);
  M5.Display.print("Scan interval: 5s");
  M5.Display.setCursor(8, 44);
  M5.Display.print("Left thresh: 45s");
  M5.Display.setCursor(8, 60);
  M5.Display.print("Persist min: 0.45");
  M5.Display.setCursor(8, 76);
  M5.Display.print("BLE window: 3s");
  M5.Display.setCursor(8, 100);
  M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
  M5.Display.print("(edit in source)");
  drawFooter("", "Back");
}

void drawAbout() {
  clearScreen();
  drawHeader("ABOUT");
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.setCursor(8, 28);
  M5.Display.print("WiFighter v0.2");
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setCursor(8, 46);
  M5.Display.print("BLE + WiFi tracker");
  M5.Display.setCursor(8, 62);
  M5.Display.print("Detects linger/left");
  M5.Display.setCursor(8, 78);
  M5.Display.print("M5StickC Plus/Plus2");
  M5.Display.setCursor(8, 100);
  M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Display.print("Ethical use only");
  drawFooter("", "Back");
}

// ---------------------------------------------------------------------------
// DEVICE UPDATE
// ---------------------------------------------------------------------------
void upsertDevice(const uint8_t* mac, int8_t rssi, bool isBLE, bool isWiFi, const char* name = nullptr) {
  DeviceRec* d = findDevice(mac);
  uint32_t now = millis();

  if (d) {
    d->rssi = rssi;
    d->lastSeen = now;
    d->hitCount++;
    d->isBLE = d->isBLE || isBLE;
    d->isWiFi = d->isWiFi || isWiFi;
    if (name && name[0] && d->name[0] == '\0') {
      strncpy(d->name, name, sizeof(d->name) - 1);
      d->name[sizeof(d->name) - 1] = '\0';
    }
    d->persistScore = calcPersistScore(*d);
    d->flaggedLeft = false;
  } else {
    if (devices.size() >= MAX_DEVICES) {
      auto it = std::min_element(devices.begin(), devices.end(),
        [](const DeviceRec& a, const DeviceRec& b) {
          return a.persistScore < b.persistScore;
        });
      devices.erase(it);
    }
    DeviceRec nd = {};
    memcpy(nd.mac, mac, 6);
    nd.rssi = rssi;
    nd.firstSeen = now;
    nd.lastSeen = now;
    nd.hitCount = 1;
    nd.isBLE = isBLE;
    nd.isWiFi = isWiFi;
    nd.flaggedLeft = false;
    nd.isTracker = false;
    strncpy(nd.vendor, lookupVendor(mac), sizeof(nd.vendor) - 1);
    if (name && name[0]) {
      strncpy(nd.name, name, sizeof(nd.name) - 1);
    }
    nd.persistScore = calcPersistScore(nd);
    devices.push_back(nd);
  }
}

// ---------------------------------------------------------------------------
// WIFI SCAN
// ---------------------------------------------------------------------------
void doWifiScan() {
  int n = WiFi.scanNetworks(false, true);
  for (int i = 0; i < n; i++) {
    uint8_t mac[6];
    parseMacString(WiFi.BSSIDstr(i), mac);
    String ssid = WiFi.SSID(i);
    upsertDevice(mac, WiFi.RSSI(i), false, true, ssid.c_str());
  }
  WiFi.scanDelete();
}

// ---------------------------------------------------------------------------
// BLE SCAN (NimBLE)
// ---------------------------------------------------------------------------
class AdvCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* adv) override {
    NimBLEAddress addr = adv->getAddress();
    uint8_t mac[6];
    String s = addr.toString().c_str();
    parseMacString(s, mac);

    const char* name = nullptr;
    if (adv->haveName()) name = adv->getName().c_str();

    upsertDevice(mac, adv->getRSSI(), true, false, name);
  }
};

NimBLEScan* pBLEScan = nullptr;

void initBle() {
  if (pBLEScan) return;
  NimBLEDevice::init("");
  NimBLEDevice::setPower(ESP_PWR_LVL_P3);
  pBLEScan = NimBLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new AdvCallbacks(), false);
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
}

void startBleScan() {
  if (!pBLEScan) initBle();
  if (pBLEScan->isScanning()) return;
  bleScanning = true;
  lastBleStart = millis();
  pBLEScan->start(BLE_SCAN_SECONDS, false);
}

void checkBleDone() {
  if (bleScanning && pBLEScan && !pBLEScan->isScanning()) {
    bleScanning = false;
  }
}

// ---------------------------------------------------------------------------
// SCAN CONTROL
// ---------------------------------------------------------------------------
void startScan() {
  scanning = true;
  lastScan = 0;
}

void stopScan() {
  scanning = false;
  if (pBLEScan && pBLEScan->isScanning()) {
    pBLEScan->stop();
  }
  bleScanning = false;
}

void processScores() {
  for (auto& d : devices) {
    d.persistScore = calcPersistScore(d);
    if (millis() - d.lastSeen > LEFT_THRESHOLD_MS) {
      d.flaggedLeft = true;
    }
  }
}

// ---------------------------------------------------------------------------
// BUTTONS
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
          case 0:
            if (scanning) stopScan();
            else startScan();
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
      if (bPressed) drawAlerts();
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
  M5.Display.setRotation(1);
  M5.Display.setBrightness(80);
  M5.Display.setTextSize(1);

  prefs.begin("wifighter", false);
  devices.reserve(MAX_DEVICES);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);

  initBle();

  clearScreen();
  drawHome();
}

void loop() {
  handleButtons();
  checkBleDone();

  uint32_t now = millis();

  if (scanning && (now - lastScan >= SCAN_INTERVAL_MS)) {
    lastScan = now;
    doWifiScan();
    startBleScan();
    processScores();
  }

  if (currentScreen == SCREEN_HOME && now - lastHomeDraw > HOME_REFRESH_MS) {
    processScores();
    drawHome();
    lastHomeDraw = now;
  }

  delay(15);
}
