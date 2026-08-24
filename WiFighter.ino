/*
 * WiFighter v0.9 - Portable WiFi/BLE Device Data Recon Tool
 * For M5StickC Plus / M5StickC Plus2
 *
 * Collects nearby WiFi AP info, BLE advertisements and tracks devices
 * that were previously seen but have left the current scan ("ghosts").
 * Designed for authorized security research and education only.
 *
 * Hardware: M5StickC Plus or Plus2 (Plus2 recommended for RF)
 * Libraries: M5Unified, M5GFX, NimBLE-Arduino
 *
 * Controls:
 *   Button A (front M5): Select / Enter / Confirm
 *   Button B (side):     Next item / Scroll / Back context
 *
 * Author: barrydinya + Grok collaboration
 * License: MIT (educational / authorized use only)
 */

#include <M5Unified.h>
#include <WiFi.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <vector>
#include <algorithm>

// ============== CONFIG ==============
#define MAX_TRACKED_DEVICES 40
#define SCAN_INTERVAL_MS    8000
#define DEVICE_TIMEOUT_MS   45000   // Consider "left" after this
#define HOME_REFRESH_MS     1000

// ============== STATE MACHINE ==============
enum AppState {
  STATE_HOME,
  STATE_MENU,
  STATE_WIFI_SCAN,
  STATE_BLE_SCAN,
  STATE_TRACK,
  STATE_DEVICE_LIST,
  STATE_SETTINGS,
  STATE_ABOUT
};

AppState currentState = STATE_HOME;
int menuIndex = 0;
unsigned long lastScanTime = 0;
unsigned long lastHomeRefresh = 0;
bool scanning = false;
int totalSeen = 0;

// ============== DEVICE TRACKING ==============
struct TrackedDevice {
  String mac;
  String name;
  int8_t rssi;
  bool isBLE;
  unsigned long lastSeen;
  int hitCount;
  bool left;          // true if timed out
};

std::vector<TrackedDevice> devices;
Preferences prefs;

// Menu items
const char* menuItems[] = {
  "WiFi Scan",
  "BLE Scan",
  "Track Mode",
  "Device List",
  "Settings",
  "About",
  "Back to Home"
};
const int MENU_COUNT = 7;

// ============== FORWARD DECLARATIONS ==============
void drawHome();
void drawMenu();
void drawHeader(const char* title);
void handleButtons();
void startWiFiScan();
void startBLEScan();
void updateTracking();
void addOrUpdateDevice(const String& mac, const String& name, int8_t rssi, bool isBLE);
String macToString(const uint8_t* mac);
void clearDevices();
void drawTrackScreen();
void drawDeviceList();
void drawSettings();
void drawAbout();
void selectMenuItem();

// ============== SETUP ==============
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(1);           // Landscape for Plus
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.fillScreen(TFT_BLACK);

  // Init WiFi in station mode for scanning
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Init NimBLE
  NimBLEDevice::init("");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9); // max TX for better range

  prefs.begin("wifighter", false);

  // Splash
  M5.Display.setTextDatum(MC_DATUM);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_CYAN);
  M5.Display.drawString("WiFighter", M5.Display.width()/2, M5.Display.height()/2 - 12);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.drawString("BLE + WiFi Tracker v0.9", M5.Display.width()/2, M5.Display.height()/2 + 10);
  delay(1600);

  currentState = STATE_HOME;
  drawHome();
}

// ============== LOOP ==============
void loop() {
  M5.update();
  handleButtons();

  unsigned long now = millis();

  switch (currentState) {
    case STATE_HOME:
      if (now - lastHomeRefresh > HOME_REFRESH_MS) {
        drawHome();
        lastHomeRefresh = now;
      }
      break;

    case STATE_TRACK:
      updateTracking();
      if (now - lastHomeRefresh > 1500) {
        drawTrackScreen();
        lastHomeRefresh = now;
      }
      break;

    case STATE_WIFI_SCAN:
    case STATE_BLE_SCAN:
      // Results screens are static until button
      break;

    default:
      break;
  }

  delay(10);
}

// ============== BUTTON HANDLING ==============
void handleButtons() {
  if (M5.BtnA.wasPressed()) {
    switch (currentState) {
      case STATE_HOME:
        currentState = STATE_MENU;
        menuIndex = 0;
        drawMenu();
        break;

      case STATE_MENU:
        selectMenuItem();
        break;

      case STATE_DEVICE_LIST:
      case STATE_TRACK:
      case STATE_ABOUT:
      case STATE_SETTINGS:
      case STATE_WIFI_SCAN:
      case STATE_BLE_SCAN:
        currentState = STATE_MENU;
        drawMenu();
        break;

      default:
        currentState = STATE_HOME;
        drawHome();
        break;
    }
  }

  if (M5.BtnB.wasPressed()) {
    switch (currentState) {
      case STATE_HOME:
        break;

      case STATE_MENU:
        menuIndex = (menuIndex + 1) % MENU_COUNT;
        drawMenu();
        break;

      case STATE_WIFI_SCAN:
      case STATE_BLE_SCAN:
      case STATE_TRACK:
      case STATE_DEVICE_LIST:
      case STATE_SETTINGS:
      case STATE_ABOUT:
        currentState = STATE_MENU;
        drawMenu();
        break;

      default:
        currentState = STATE_HOME;
        drawHome();
        break;
    }
  }
}

void selectMenuItem() {
  switch (menuIndex) {
    case 0: // WiFi Scan
      currentState = STATE_WIFI_SCAN;
      startWiFiScan();
      break;
    case 1: // BLE Scan
      currentState = STATE_BLE_SCAN;
      startBLEScan();
      break;
    case 2: // Track Mode
      currentState = STATE_TRACK;
      lastScanTime = 0;
      drawTrackScreen();
      break;
    case 3: // Device List
      currentState = STATE_DEVICE_LIST;
      drawDeviceList();
      break;
    case 4: // Settings
      currentState = STATE_SETTINGS;
      drawSettings();
      break;
    case 5: // About
      currentState = STATE_ABOUT;
      drawAbout();
      break;
    case 6: // Back to Home
      currentState = STATE_HOME;
      drawHome();
      break;
  }
}

// ============== DRAWING ==============
void drawHeader(const char* title) {
  M5.Display.fillRect(0, 0, M5.Display.width(), 16, TFT_NAVY);
  M5.Display.setTextColor(TFT_WHITE, TFT_NAVY);
  M5.Display.setTextDatum(TL_DATUM);
  M5.Display.setCursor(4, 4);
  M5.Display.print(title);

  // Battery
  int bat = M5.Power.getBatteryLevel();
  M5.Display.setTextDatum(TR_DATUM);
  M5.Display.setCursor(M5.Display.width() - 4, 4);
  M5.Display.printf("%d%%", bat);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
}

void drawHome() {
  M5.Display.fillScreen(TFT_BLACK);
  drawHeader("WiFighter");

  M5.Display.setTextDatum(TL_DATUM);
  M5.Display.setCursor(8, 26);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_CYAN);
  M5.Display.print("HOME");

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setCursor(8, 52);
  M5.Display.printf("Tracked : %d", (int)devices.size());

  int active = 0;
  int ghosts = 0;
  for (auto& d : devices) {
    if (d.left) ghosts++;
    else active++;
  }
  M5.Display.setCursor(8, 66);
  M5.Display.printf("Active  : %d", active);
  M5.Display.setCursor(8, 80);
  M5.Display.setTextColor(TFT_ORANGE);
  M5.Display.printf("Ghosts  : %d", ghosts);

  M5.Display.setTextColor(TFT_LIGHTGREY);
  M5.Display.setCursor(8, 96);
  unsigned long age = (millis() - lastScanTime) / 1000;
  if (lastScanTime == 0) M5.Display.print("Last scan: never");
  else M5.Display.printf("Last scan: %lus ago", age);

  M5.Display.setCursor(8, 112);
  M5.Display.setTextColor(TFT_YELLOW);
  M5.Display.print("A: Menu");

  // Status bar
  M5.Display.fillRect(0, M5.Display.height() - 12, M5.Display.width(), 12, TFT_DARKGREY);
  M5.Display.setTextColor(TFT_GREEN, TFT_DARKGREY);
  M5.Display.setCursor(4, M5.Display.height() - 10);
  M5.Display.print("Ready - authorized use only");
}

void drawMenu() {
  M5.Display.fillScreen(TFT_BLACK);
  drawHeader("MENU");

  for (int i = 0; i < MENU_COUNT; i++) {
    int y = 22 + i * 15;
    if (i == menuIndex) {
      M5.Display.fillRect(0, y - 2, M5.Display.width(), 15, TFT_DARKGREEN);
      M5.Display.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    } else {
      M5.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    }
    M5.Display.setCursor(8, y);
    M5.Display.print(menuItems[i]);
  }

  M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
  M5.Display.setCursor(4, M5.Display.height() - 12);
  M5.Display.print("A:Select  B:Next");
}

void drawTrackScreen() {
  M5.Display.fillScreen(TFT_BLACK);
  drawHeader("TRACK MODE");

  M5.Display.setTextDatum(TL_DATUM);
  M5.Display.setCursor(8, 22);
  M5.Display.setTextColor(TFT_GREEN);
  M5.Display.print("Monitoring...");

  int y = 40;
  int shown = 0;
  for (auto& d : devices) {
    if (shown >= 5) break;
    if (d.left) {
      M5.Display.setTextColor(TFT_RED);
    } else {
      M5.Display.setTextColor(TFT_WHITE);
    }
    M5.Display.setCursor(4, y);
    String shortMac = d.mac.length() > 8 ? d.mac.substring(d.mac.length() - 8) : d.mac;
    M5.Display.printf("%s %s %d", d.isBLE ? "B" : "W", shortMac.c_str(), d.rssi);
    y += 14;
    shown++;
  }

  if (devices.empty()) {
    M5.Display.setTextColor(TFT_DARKGREY);
    M5.Display.setCursor(8, 50);
    M5.Display.print("No devices tracked yet");
  }

  M5.Display.setTextColor(TFT_YELLOW);
  M5.Display.setCursor(4, M5.Display.height() - 12);
  M5.Display.print("A/B: Menu");
}

void drawDeviceList() {
  M5.Display.fillScreen(TFT_BLACK);
  drawHeader("DEVICES");

  int y = 22;
  int count = 0;
  for (auto& d : devices) {
    if (count >= 7) break;
    M5.Display.setTextColor(d.left ? TFT_RED : TFT_CYAN);
    M5.Display.setCursor(4, y);
    String label = d.name.length() > 0 ? d.name : d.mac.substring(9);
    if (label.length() > 12) label = label.substring(0, 12);
    M5.Display.printf("%s %d", label.c_str(), d.rssi);
    y += 14;
    count++;
  }

  if (devices.empty()) {
    M5.Display.setTextColor(TFT_DARKGREY);
    M5.Display.setCursor(8, 50);
    M5.Display.print("No devices yet");
  }

  M5.Display.setTextColor(TFT_YELLOW);
  M5.Display.setCursor(4, M5.Display.height() - 12);
  M5.Display.print("A/B: Back");
}

void drawSettings() {
  M5.Display.fillScreen(TFT_BLACK);
  drawHeader("SETTINGS");

  M5.Display.setCursor(8, 28);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.print("Timeout    : 45s");
  M5.Display.setCursor(8, 44);
  M5.Display.print("Max devices: 40");
  M5.Display.setCursor(8, 60);
  M5.Display.print("Scan int.  : 8s");
  M5.Display.setCursor(8, 80);
  M5.Display.setTextColor(TFT_ORANGE);
  M5.Display.print("More options in next");
  M5.Display.setCursor(8, 94);
  M5.Display.print("iteration (v1.0)");

  M5.Display.setTextColor(TFT_YELLOW);
  M5.Display.setCursor(4, M5.Display.height() - 12);
  M5.Display.print("A/B: Back");
}

void drawAbout() {
  M5.Display.fillScreen(TFT_BLACK);
  drawHeader("ABOUT");

  M5.Display.setTextDatum(MC_DATUM);
  M5.Display.setTextColor(TFT_CYAN);
  M5.Display.drawString("WiFighter v0.9", M5.Display.width()/2, 32);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.drawString("BLE + WiFi Tracker", M5.Display.width()/2, 50);
  M5.Display.setTextColor(TFT_LIGHTGREY);
  M5.Display.drawString("M5StickC Plus/Plus2", M5.Display.width()/2, 68);
  M5.Display.drawString("Authorized use only", M5.Display.width()/2, 86);

  M5.Display.setTextDatum(TL_DATUM);
  M5.Display.setTextColor(TFT_YELLOW);
  M5.Display.setCursor(4, M5.Display.height() - 12);
  M5.Display.print("A/B: Back");
}

// ============== SCANNING ==============
void startWiFiScan() {
  M5.Display.fillScreen(TFT_BLACK);
  drawHeader("WIFI SCAN");
  M5.Display.setCursor(8, 40);
  M5.Display.setTextColor(TFT_YELLOW);
  M5.Display.print("Scanning WiFi...");

  int n = WiFi.scanNetworks(false, true);

  M5.Display.fillScreen(TFT_BLACK);
  drawHeader("WIFI RESULTS");

  int y = 22;
  for (int i = 0; i < n && i < 6; ++i) {
    String ssid = WiFi.SSID(i);
    int32_t rssi = WiFi.RSSI(i);
    String bssid = WiFi.BSSIDstr(i);

    addOrUpdateDevice(bssid, ssid, rssi, false);

    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setCursor(4, y);
    if (ssid.length() > 14) ssid = ssid.substring(0, 14);
    M5.Display.printf("%s %d", ssid.c_str(), rssi);
    y += 14;
  }

  totalSeen += n;
  lastScanTime = millis();
  WiFi.scanDelete();

  M5.Display.setTextColor(TFT_YELLOW);
  M5.Display.setCursor(4, M5.Display.height() - 12);
  M5.Display.print("A: Menu");
}

class BLEScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
    String mac = advertisedDevice->getAddress().toString().c_str();
    String name = advertisedDevice->getName().c_str();
    int8_t rssi = advertisedDevice->getRSSI();
    addOrUpdateDevice(mac, name, rssi, true);
  }
};

void startBLEScan() {
  M5.Display.fillScreen(TFT_BLACK);
  drawHeader("BLE SCAN");
  M5.Display.setCursor(8, 40);
  M5.Display.setTextColor(TFT_YELLOW);
  M5.Display.print("Scanning BLE...");

  NimBLEScan* pScan = NimBLEDevice::getScan();
  pScan->setScanCallbacks(new BLEScanCallbacks(), false);
  pScan->setActiveScan(true);
  pScan->setInterval(100);
  pScan->setWindow(99);
  pScan->start(4, false);

  delay(4500);

  M5.Display.fillScreen(TFT_BLACK);
  drawHeader("BLE RESULTS");

  int y = 22;
  int shown = 0;
  for (auto& d : devices) {
    if (!d.isBLE) continue;
    if (shown >= 6) break;
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setCursor(4, y);
    String label = d.name.length() > 0 ? d.name : d.mac.substring(9);
    if (label.length() > 14) label = label.substring(0, 14);
    M5.Display.printf("%s %d", label.c_str(), d.rssi);
    y += 14;
    shown++;
  }

  lastScanTime = millis();
  M5.Display.setTextColor(TFT_YELLOW);
  M5.Display.setCursor(4, M5.Display.height() - 12);
  M5.Display.print("A: Menu");
}

void updateTracking() {
  unsigned long now = millis();
  if (now - lastScanTime < 5000) return;

  // Quick BLE scan
  NimBLEScan* pScan = NimBLEDevice::getScan();
  pScan->setScanCallbacks(new BLEScanCallbacks(), false);
  pScan->setActiveScan(true);
  pScan->start(2, false);
  delay(2200);

  // Light WiFi
  int n = WiFi.scanNetworks(false, true, false, 300);
  for (int i = 0; i < n; ++i) {
    addOrUpdateDevice(WiFi.BSSIDstr(i), WiFi.SSID(i), WiFi.RSSI(i), false);
  }
  WiFi.scanDelete();

  // Mark left devices
  for (auto& d : devices) {
    if (now - d.lastSeen > DEVICE_TIMEOUT_MS) {
      d.left = true;
    }
  }

  lastScanTime = now;
}

void addOrUpdateDevice(const String& mac, const String& name, int8_t rssi, bool isBLE) {
  for (auto& d : devices) {
    if (d.mac.equalsIgnoreCase(mac)) {
      d.rssi = rssi;
      d.lastSeen = millis();
      d.hitCount++;
      d.left = false;
      if (name.length() > 0 && d.name.length() == 0) d.name = name;
      return;
    }
  }

  if (devices.size() >= MAX_TRACKED_DEVICES) {
    // Remove oldest left device
    for (auto it = devices.begin(); it != devices.end(); ++it) {
      if (it->left) {
        devices.erase(it);
        break;
      }
    }
    if (devices.size() >= MAX_TRACKED_DEVICES) return;
  }

  TrackedDevice nd;
  nd.mac = mac;
  nd.name = name;
  nd.rssi = rssi;
  nd.isBLE = isBLE;
  nd.lastSeen = millis();
  nd.hitCount = 1;
  nd.left = false;
  devices.push_back(nd);
}

String macToString(const uint8_t* mac) {
  char buf[18];
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

void clearDevices() {
  devices.clear();
}
