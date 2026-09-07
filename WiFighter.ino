/*
 * WiFighter v1.0 - Portable WiFi/BLE Device Data Recon Tool
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
#define MAX_TRACKED_DEVICES 48
#define SCAN_INTERVAL_MS    7000
#define DEVICE_TIMEOUT_MS   40000   // Consider "left" after this
#define HOME_REFRESH_MS     1200
#define PROXIMITY_RSSI      -55     // Strong signal threshold for note
#define BRIGHTNESS_DEFAULT  80

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
int settingsIndex = 0;
unsigned long lastScanTime = 0;
unsigned long lastHomeRefresh = 0;
unsigned long lastButtonTime = 0;
bool scanning = false;
int totalSeen = 0;
bool autoSleep = false;
int brightness = BRIGHTNESS_DEFAULT;

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

const char* settingsItems[] = {
  "Clear Devices",
  "Export Serial",
  "Toggle AutoSleep",
  "Brightness +",
  "Brightness -",
  "Back"
};
const int SETTINGS_COUNT = 6;

// Re-usable BLE callback (avoid repeated new)
class BLEScanCallbacks : public NimBLEScanCallbacks {
public:
  void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override;
};

BLEScanCallbacks bleCallbacks;

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
void selectSettingsItem();
void saveDevices();
void loadDevices();
void exportSerial();
void applyBrightness();

// ============== SETUP ==============
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(1);           // Landscape for Plus
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.fillScreen(TFT_BLACK);
  applyBrightness();

  // Init WiFi in station mode for scanning
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);

  // Init NimBLE once
  NimBLEDevice::init("");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9); // max TX for better range

  prefs.begin("wifighter", false);
  loadDevices();

  // Splash
  M5.Display.setTextDatum(MC_DATUM);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_CYAN);
  M5.Display.drawString("WiFighter", M5.Display.width()/2, M5.Display.height()/2 - 14);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.drawString("BLE + WiFi Tracker v1.0", M5.Display.width()/2, M5.Display.height()/2 + 8);
  M5.Display.setTextColor(TFT_DARKGREY);
  M5.Display.drawString("Authorized use only", M5.Display.width()/2, M5.Display.height()/2 + 24);
  delay(1800);

  currentState = STATE_HOME;
  drawHome();
  lastButtonTime = millis();
}

// ============== LOOP ==============
void loop() {
  M5.update();
  handleButtons();

  unsigned long now = millis();

  // Simple inactivity dim / sleep hint
  if (autoSleep && (now - lastButtonTime > 90000)) {
    M5.Display.setBrightness(20);
  }

  switch (currentState) {
    case STATE_HOME:
      if (now - lastHomeRefresh > HOME_REFRESH_MS) {
        drawHome();
        lastHomeRefresh = now;
      }
      break;

    case STATE_TRACK:
      updateTracking();
      if (now - lastHomeRefresh > 1600) {
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

  delay(12);
}

// ============== BUTTON HANDLING ==============
void handleButtons() {
  if (M5.BtnA.wasPressed()) {
    lastButtonTime = millis();
    applyBrightness(); // restore if dimmed
    switch (currentState) {
      case STATE_HOME:
        currentState = STATE_MENU;
        menuIndex = 0;
        drawMenu();
        break;

      case STATE_MENU:
        selectMenuItem();
        break;

      case STATE_SETTINGS:
        selectSettingsItem();
        break;

      case STATE_DEVICE_LIST:
      case STATE_TRACK:
      case STATE_ABOUT:
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
    lastButtonTime = millis();
    applyBrightness();
    switch (currentState) {
      case STATE_HOME:
        break;

      case STATE_MENU:
        menuIndex = (menuIndex + 1) % MENU_COUNT;
        drawMenu();
        break;

      case STATE_SETTINGS:
        settingsIndex = (settingsIndex + 1) % SETTINGS_COUNT;
        drawSettings();
        break;

      case STATE_WIFI_SCAN:
      case STATE_BLE_SCAN:
      case STATE_TRACK:
      case STATE_DEVICE_LIST:
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
      settingsIndex = 0;
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

void selectSettingsItem() {
  switch (settingsIndex) {
    case 0: // Clear
      clearDevices();
      saveDevices();
      M5.Display.fillScreen(TFT_BLACK);
      drawHeader("SETTINGS");
      M5.Display.setCursor(8, 50);
      M5.Display.setTextColor(TFT_GREEN);
      M5.Display.print("Devices cleared");
      delay(900);
      drawSettings();
      break;
    case 1: // Export
      exportSerial();
      M5.Display.fillScreen(TFT_BLACK);
      drawHeader("SETTINGS");
      M5.Display.setCursor(8, 50);
      M5.Display.setTextColor(TFT_GREEN);
      M5.Display.print("Exported to Serial");
      delay(900);
      drawSettings();
      break;
    case 2: // AutoSleep toggle
      autoSleep = !autoSleep;
      drawSettings();
      break;
    case 3: // Bright +
      brightness = min(100, brightness + 15);
      applyBrightness();
      drawSettings();
      break;
    case 4: // Bright -
      brightness = max(15, brightness - 15);
      applyBrightness();
      drawSettings();
      break;
    case 5: // Back
      currentState = STATE_MENU;
      drawMenu();
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
  M5.Display.setCursor(8, 24);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_CYAN);
  M5.Display.print("HOME");

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setCursor(8, 48);
  M5.Display.printf("Tracked : %d", (int)devices.size());

  int active = 0;
  int ghosts = 0;
  for (auto& d : devices) {
    if (d.left) ghosts++;
    else active++;
  }
  M5.Display.setCursor(8, 62);
  M5.Display.printf("Active  : %d", active);
  M5.Display.setCursor(8, 76);
  M5.Display.setTextColor(TFT_ORANGE);
  M5.Display.printf("Ghosts  : %d", ghosts);

  M5.Display.setTextColor(TFT_LIGHTGREY);
  M5.Display.setCursor(8, 92);
  unsigned long age = (millis() - lastScanTime) / 1000;
  if (lastScanTime == 0) M5.Display.print("Last scan: never");
  else M5.Display.printf("Last scan: %lus ago", age);

  M5.Display.setCursor(8, 108);
  M5.Display.setTextColor(TFT_YELLOW);
  M5.Display.print("A: Menu");

  // Status bar
  M5.Display.fillRect(0, M5.Display.height() - 12, M5.Display.width(), 12, TFT_DARKGREY);
  M5.Display.setTextColor(TFT_GREEN, TFT_DARKGREY);
  M5.Display.setCursor(4, M5.Display.height() - 10);
  M5.Display.print("v1.0 - authorized use only");
}

void drawMenu() {
  M5.Display.fillScreen(TFT_BLACK);
  drawHeader("MENU");

  for (int i = 0; i < MENU_COUNT; i++) {
    int y = 20 + i * 14;
    if (i == menuIndex) {
      M5.Display.fillRect(0, y - 2, M5.Display.width(), 14, TFT_DARKGREEN);
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
  M5.Display.setCursor(8, 20);
  M5.Display.setTextColor(TFT_GREEN);
  M5.Display.print("Hybrid monitoring...");

  int y = 36;
  int shown = 0;
  for (auto& d : devices) {
    if (shown >= 5) break;
    if (d.left) {
      M5.Display.setTextColor(TFT_RED);
    } else if (d.rssi >= PROXIMITY_RSSI) {
      M5.Display.setTextColor(TFT_YELLOW); // close
    } else {
      M5.Display.setTextColor(TFT_WHITE);
    }
    M5.Display.setCursor(4, y);
    String shortMac = d.mac.length() > 8 ? d.mac.substring(d.mac.length() - 8) : d.mac;
    M5.Display.printf("%s %s %d", d.isBLE ? "B" : "W", shortMac.c_str(), d.rssi);
    y += 13;
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

  int y = 20;
  int count = 0;
  for (auto& d : devices) {
    if (count >= 7) break;
    M5.Display.setTextColor(d.left ? TFT_RED : (d.rssi >= PROXIMITY_RSSI ? TFT_YELLOW : TFT_CYAN));
    M5.Display.setCursor(4, y);
    String label = d.name.length() > 0 ? d.name : d.mac.substring(9);
    if (label.length() > 12) label = label.substring(0, 12);
    M5.Display.printf("%s %d", label.c_str(), d.rssi);
    y += 13;
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

  for (int i = 0; i < SETTINGS_COUNT; i++) {
    int y = 20 + i * 14;
    if (i == settingsIndex) {
      M5.Display.fillRect(0, y - 2, M5.Display.width(), 14, TFT_DARKGREEN);
      M5.Display.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    } else {
      M5.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    }
    M5.Display.setCursor(8, y);
    if (i == 2) {
      M5.Display.printf("%s: %s", settingsItems[i], autoSleep ? "ON" : "OFF");
    } else if (i == 3 || i == 4) {
      M5.Display.printf("%s (%d)", settingsItems[i], brightness);
    } else {
      M5.Display.print(settingsItems[i]);
    }
  }

  M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
  M5.Display.setCursor(4, M5.Display.height() - 12);
  M5.Display.print("A:Action  B:Next");
}

void drawAbout() {
  M5.Display.fillScreen(TFT_BLACK);
  drawHeader("ABOUT");

  M5.Display.setTextDatum(MC_DATUM);
  M5.Display.setTextColor(TFT_CYAN);
  M5.Display.drawString("WiFighter v1.0", M5.Display.width()/2, 30);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.drawString("BLE + WiFi Tracker", M5.Display.width()/2, 48);
  M5.Display.setTextColor(TFT_LIGHTGREY);
  M5.Display.drawString("M5StickC Plus/Plus2", M5.Display.width()/2, 66);
  M5.Display.drawString("Authorized use only", M5.Display.width()/2, 84);

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

  int y = 20;
  for (int i = 0; i < n && i < 6; ++i) {
    String ssid = WiFi.SSID(i);
    int32_t rssi = WiFi.RSSI(i);
    String bssid = WiFi.BSSIDstr(i);

    addOrUpdateDevice(bssid, ssid, rssi, false);

    M5.Display.setTextColor(rssi >= PROXIMITY_RSSI ? TFT_YELLOW : TFT_WHITE);
    M5.Display.setCursor(4, y);
    if (ssid.length() > 14) ssid = ssid.substring(0, 14);
    M5.Display.printf("%s %d", ssid.c_str(), rssi);
    y += 13;
  }

  totalSeen += n;
  lastScanTime = millis();
  WiFi.scanDelete();
  saveDevices();

  M5.Display.setTextColor(TFT_YELLOW);
  M5.Display.setCursor(4, M5.Display.height() - 12);
  M5.Display.print("A: Menu");
}

void BLEScanCallbacks::onResult(const NimBLEAdvertisedDevice* advertisedDevice) {
  String mac = advertisedDevice->getAddress().toString().c_str();
  String name = advertisedDevice->getName().c_str();
  int8_t rssi = advertisedDevice->getRSSI();
  addOrUpdateDevice(mac, name, rssi, true);
}

void startBLEScan() {
  M5.Display.fillScreen(TFT_BLACK);
  drawHeader("BLE SCAN");
  M5.Display.setCursor(8, 40);
  M5.Display.setTextColor(TFT_YELLOW);
  M5.Display.print("Scanning BLE...");

  NimBLEScan* pScan = NimBLEDevice::getScan();
  pScan->setScanCallbacks(&bleCallbacks, false);
  pScan->setActiveScan(true);
  pScan->setInterval(100);
  pScan->setWindow(99);
  pScan->start(4, false);

  delay(4500);
  pScan->stop();

  M5.Display.fillScreen(TFT_BLACK);
  drawHeader("BLE RESULTS");

  int y = 20;
  int shown = 0;
  for (auto& d : devices) {
    if (!d.isBLE) continue;
    if (shown >= 6) break;
    M5.Display.setTextColor(d.rssi >= PROXIMITY_RSSI ? TFT_YELLOW : TFT_WHITE);
    M5.Display.setCursor(4, y);
    String label = d.name.length() > 0 ? d.name : d.mac.substring(9);
    if (label.length() > 14) label = label.substring(0, 14);
    M5.Display.printf("%s %d", label.c_str(), d.rssi);
    y += 13;
    shown++;
  }

  lastScanTime = millis();
  saveDevices();
  M5.Display.setTextColor(TFT_YELLOW);
  M5.Display.setCursor(4, M5.Display.height() - 12);
  M5.Display.print("A: Menu");
}

void updateTracking() {
  unsigned long now = millis();
  if (now - lastScanTime < 4500) return;

  // Quick BLE scan
  NimBLEScan* pScan = NimBLEDevice::getScan();
  pScan->setScanCallbacks(&bleCallbacks, false);
  pScan->setActiveScan(true);
  pScan->start(2, false);
  delay(2300);
  pScan->stop();

  // Light WiFi
  int n = WiFi.scanNetworks(false, true, false, 250);
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
  saveDevices();
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
    if (devices.size() >= MAX_TRACKED_DEVICES) {
      // fallback: drop oldest
      devices.erase(devices.begin());
    }
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

void applyBrightness() {
  M5.Display.setBrightness(brightness);
}

// Simple NVS persistence (store count + basic fields as strings)
void saveDevices() {
  prefs.putInt("count", (int)devices.size());
  for (size_t i = 0; i < devices.size() && i < MAX_TRACKED_DEVICES; i++) {
    String key = "d" + String(i);
    String val = devices[i].mac + "|" + devices[i].name + "|" +
                 String(devices[i].rssi) + "|" + (devices[i].isBLE ? "1" : "0") + "|" +
                 String(devices[i].hitCount) + "|" + (devices[i].left ? "1" : "0");
    prefs.putString(key.c_str(), val);
  }
}

void loadDevices() {
  devices.clear();
  int count = prefs.getInt("count", 0);
  if (count > MAX_TRACKED_DEVICES) count = MAX_TRACKED_DEVICES;
  for (int i = 0; i < count; i++) {
    String key = "d" + String(i);
    String val = prefs.getString(key.c_str(), "");
    if (val.length() == 0) continue;
    // Parse mac|name|rssi|isBLE|hits|left
    int p1 = val.indexOf('|');
    int p2 = val.indexOf('|', p1 + 1);
    int p3 = val.indexOf('|', p2 + 1);
    int p4 = val.indexOf('|', p3 + 1);
    int p5 = val.indexOf('|', p4 + 1);
    if (p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0 || p5 < 0) continue;
    TrackedDevice d;
    d.mac = val.substring(0, p1);
    d.name = val.substring(p1 + 1, p2);
    d.rssi = val.substring(p2 + 1, p3).toInt();
    d.isBLE = val.substring(p3 + 1, p4).toInt() == 1;
    d.hitCount = val.substring(p4 + 1, p5).toInt();
    d.left = val.substring(p5 + 1).toInt() == 1;
    d.lastSeen = millis(); // reset age on load
    devices.push_back(d);
  }
}

void exportSerial() {
  Serial.println("=== WiFighter Device Export ===");
  Serial.printf("Count: %d\n", (int)devices.size());
  for (auto& d : devices) {
    Serial.printf("%s,%s,%d,%s,hits=%d,left=%d\n",
                  d.mac.c_str(),
                  d.name.c_str(),
                  d.rssi,
                  d.isBLE ? "BLE" : "WiFi",
                  d.hitCount,
                  d.left ? 1 : 0);
  }
  Serial.println("=== End ===");
}
