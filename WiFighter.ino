/*
 * WiFighter v1.1 - Portable WiFi/BLE Device Data Recon Tool
 * For M5StickC Plus / M5StickC Plus2
 *
 * Collects nearby WiFi AP info, BLE advertisements and tracks devices
 * that were previously seen but have left the current scan ("ghosts").
 * Pulls residual identifiers and signal history for devices that have left
 * WiFi/BLE range.
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
#define DEVICE_TIMEOUT_MS   45000   // Consider "left" after this
#define HOME_REFRESH_MS     1100
#define PROXIMITY_RSSI      -52     // Strong signal threshold
#define BRIGHTNESS_DEFAULT  85
#define VERSION_STR         "v1.1"

// ============== STATE MACHINE ==============
enum AppState {
  STATE_HOME,
  STATE_MENU,
  STATE_WIFI_SCAN,
  STATE_BLE_SCAN,
  STATE_TRACK,
  STATE_DEVICE_LIST,
  STATE_GHOSTS,
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
bool showOnlyGhosts = false;

// ============== DEVICE TRACKING ==============
struct TrackedDevice {
  String mac;
  String name;
  String vendor;      // simple OUI hint
  int8_t rssi;
  int8_t bestRssi;    // strongest ever seen
  bool isBLE;
  unsigned long lastSeen;
  unsigned long firstSeen;
  int hitCount;
  bool left;          // true if timed out (ghost)
};

std::vector<TrackedDevice> devices;
Preferences prefs;

// Menu items - expanded
const char* menuItems[] = {
  "WiFi Scan",
  "BLE Scan",
  "Track Mode",
  "Device List",
  "Ghosts Only",
  "Settings",
  "About",
  "Back to Home"
};
const int MENU_COUNT = 8;

const char* settingsItems[] = {
  "Clear All Devices",
  "Clear Ghosts Only",
  "Export Serial",
  "Toggle AutoSleep",
  "Brightness +",
  "Brightness -",
  "Back"
};
const int SETTINGS_COUNT = 7;

// Simple common OUI prefixes for vendor hint (first 3 bytes)
struct OuiEntry {
  const char* prefix;
  const char* name;
};
const OuiEntry ouiTable[] = {
  {"00:1A:11", "Google"},
  {"00:1B:63", "Apple"},
  {"18:65:90", "Apple"},
  {"28:CF:E9", "Apple"},
  {"3C:22:FB", "Apple"},
  {"40:B0:34", "Apple"},
  {"48:E9:F1", "Apple"},
  {"54:72:4F", "Apple"},
  {"64:B9:E8", "Apple"},
  {"78:31:C1", "Apple"},
  {"88:63:DF", "Apple"},
  {"A4:83:E7", "Apple"},
  {"AC:BC:32", "Apple"},
  {"DC:A9:04", "Apple"},
  {"F0:18:98", "Apple"},
  {"F0:99:BF", "Apple"},
  {"B8:27:EB", "RPi"},
  {"DC:A6:32", "RPi"},
  {"E4:5F:01", "RPi"},
  {"28:CD:C1", "RPi"},
  {"00:50:56", "VMware"},
  {"00:0C:29", "VMware"},
  {"00:1B:21", "Intel"},
  {"00:1E:67", "Intel"},
  {"3C:97:0E", "Intel"},
  {"68:05:CA", "Intel"},
  {"A0:36:9F", "Intel"},
  {"00:E0:4C", "Realtek"},
  {"00:E0:18", "ASUS"},
  {"00:1B:FC", "ASUS"},
  {"00:22:15", "ASUS"},
  {"04:92:26", "ASUS"},
  {"2C:4D:54", "ASUS"},
  {"00:17:C9", "Samsung"},
  {"00:1D:25", "Samsung"},
  {"00:21:19", "Samsung"},
  {"00:23:39", "Samsung"},
  {"08:08:C2", "Samsung"},
  {"14:89:FD", "Samsung"},
  {"18:3A:2D", "Samsung"},
  {"28:98:7B", "Samsung"},
  {"34:23:BA", "Samsung"},
  {"38:01:97", "Samsung"},
  {"50:32:75", "Samsung"},
  {"5C:F6:DC", "Samsung"},
  {"64:77:91", "Samsung"},
  {"78:1F:DB", "Samsung"},
  {"84:25:DB", "Samsung"},
  {"94:35:0A", "Samsung"},
  {"A0:75:91", "Samsung"},
  {"B0:47:BF", "Samsung"},
  {"BC:14:85", "Samsung"},
  {"C0:97:27", "Samsung"},
  {"D0:17:6A", "Samsung"},
  {"E0:99:71", "Samsung"},
  {"F0:25:B7", "Samsung"},
  {"FC:A1:3E", "Samsung"},
  {"24:0A:C4", "Espressif"},
  {"24:6F:28", "Espressif"},
  {"30:AE:A4", "Espressif"},
  {"3C:71:BF", "Espressif"},
  {"4C:11:AE", "Espressif"},
  {"5C:CF:7F", "Espressif"},
  {"68:C6:3A", "Espressif"},
  {"7C:9E:BD", "Espressif"},
  {"84:CC:A8", "Espressif"},
  {"8C:AA:B5", "Espressif"},
  {"94:B9:7E", "Espressif"},
  {"A4:CF:12", "Espressif"},
  {"AC:67:B2", "Espressif"},
  {"B4:E6:2D", "Espressif"},
  {"C4:4F:33", "Espressif"},
  {"CC:50:E3", "Espressif"},
  {"DC:4F:22", "Espressif"},
  {"E8:DB:84", "Espressif"},
  {"F4:CF:A2", "Espressif"},
};
const int OUI_COUNT = sizeof(ouiTable) / sizeof(ouiTable[0]);

// Re-usable BLE callback
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
String lookupVendor(const String& mac);
void clearDevices();
void clearGhosts();
void drawTrackScreen();
void drawDeviceList(bool ghostsOnly);
void drawSettings();
void drawAbout();
void selectMenuItem();
void selectSettingsItem();
void saveDevices();
void loadDevices();
void exportSerial();
void applyBrightness();
void playTone(int freq, int duration);

// ============== SETUP ==============
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(1);           // Landscape
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.fillScreen(TFT_BLACK);
  applyBrightness();

  // WiFi station for scanning only
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(80);

  // NimBLE
  NimBLEDevice::init("");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  prefs.begin("wifighter", false);
  loadDevices();

  // Splash
  M5.Display.setTextDatum(MC_DATUM);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_CYAN);
  M5.Display.drawString("WiFighter", M5.Display.width()/2, M5.Display.height()/2 - 16);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.drawString("BLE + WiFi Tracker " VERSION_STR, M5.Display.width()/2, M5.Display.height()/2 + 6);
  M5.Display.setTextColor(TFT_DARKGREY);
  M5.Display.drawString("Authorized use only", M5.Display.width()/2, M5.Display.height()/2 + 22);
  delay(1600);

  // Optional short beep
  playTone(1200, 60);

  currentState = STATE_HOME;
  drawHome();
  lastButtonTime = millis();
}

// ============== LOOP ==============
void loop() {
  M5.update();
  handleButtons();

  unsigned long now = millis();

  // Dim on inactivity
  if (autoSleep && (now - lastButtonTime > 85000)) {
    M5.Display.setBrightness(18);
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
      if (now - lastHomeRefresh > 1500) {
        drawTrackScreen();
        lastHomeRefresh = now;
      }
      break;

    default:
      break;
  }

  delay(10);
}

// ============== BUTTON HANDLING ==============
void handleButtons() {
  if (M5.BtnA.wasPressed()) {
    lastButtonTime = millis();
    applyBrightness();
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
      case STATE_GHOSTS:
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
        // Quick jump to Track
        currentState = STATE_TRACK;
        lastScanTime = 0;
        drawTrackScreen();
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
      case STATE_GHOSTS:
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
      showOnlyGhosts = false;
      drawDeviceList(false);
      break;
    case 4: // Ghosts Only
      currentState = STATE_GHOSTS;
      showOnlyGhosts = true;
      drawDeviceList(true);
      break;
    case 5: // Settings
      currentState = STATE_SETTINGS;
      settingsIndex = 0;
      drawSettings();
      break;
    case 6: // About
      currentState = STATE_ABOUT;
      drawAbout();
      break;
    case 7: // Back to Home
      currentState = STATE_HOME;
      drawHome();
      break;
  }
}

void selectSettingsItem() {
  switch (settingsIndex) {
    case 0: // Clear All
      clearDevices();
      saveDevices();
      M5.Display.fillScreen(TFT_BLACK);
      drawHeader("SETTINGS");
      M5.Display.setCursor(8, 50);
      M5.Display.setTextColor(TFT_GREEN);
      M5.Display.print("All devices cleared");
      playTone(800, 80);
      delay(700);
      drawSettings();
      break;
    case 1: // Clear Ghosts
      clearGhosts();
      saveDevices();
      M5.Display.fillScreen(TFT_BLACK);
      drawHeader("SETTINGS");
      M5.Display.setCursor(8, 50);
      M5.Display.setTextColor(TFT_GREEN);
      M5.Display.print("Ghosts cleared");
      playTone(900, 60);
      delay(700);
      drawSettings();
      break;
    case 2: // Export
      exportSerial();
      M5.Display.fillScreen(TFT_BLACK);
      drawHeader("SETTINGS");
      M5.Display.setCursor(8, 50);
      M5.Display.setTextColor(TFT_GREEN);
      M5.Display.print("Exported to Serial");
      delay(700);
      drawSettings();
      break;
    case 3: // AutoSleep
      autoSleep = !autoSleep;
      drawSettings();
      break;
    case 4: // Bright +
      brightness = min(100, brightness + 12);
      applyBrightness();
      drawSettings();
      break;
    case 5: // Bright -
      brightness = max(12, brightness - 12);
      applyBrightness();
      drawSettings();
      break;
    case 6: // Back
      currentState = STATE_MENU;
      drawMenu();
      break;
  }
}

// ============== DRAWING ==============
void drawHeader(const char* title) {
  M5.Display.fillRect(0, 0, M5.Display.width(), 15, TFT_NAVY);
  M5.Display.setTextColor(TFT_WHITE, TFT_NAVY);
  M5.Display.setTextDatum(TL_DATUM);
  M5.Display.setCursor(3, 3);
  M5.Display.print(title);

  int bat = M5.Power.getBatteryLevel();
  M5.Display.setTextDatum(TR_DATUM);
  M5.Display.setCursor(M5.Display.width() - 3, 3);
  if (bat > 60) M5.Display.setTextColor(TFT_GREEN, TFT_NAVY);
  else if (bat > 25) M5.Display.setTextColor(TFT_YELLOW, TFT_NAVY);
  else M5.Display.setTextColor(TFT_RED, TFT_NAVY);
  M5.Display.printf("%d%%", bat);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
}

void drawHome() {
  M5.Display.fillScreen(TFT_BLACK);
  drawHeader("WiFighter " VERSION_STR);

  // Title block
  M5.Display.setTextDatum(TL_DATUM);
  M5.Display.setCursor(6, 20);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_CYAN);
  M5.Display.print("HOME");

  // Counts
  int active = 0;
  int ghosts = 0;
  int bleCount = 0;
  int wifiCount = 0;
  for (auto& d : devices) {
    if (d.left) ghosts++;
    else active++;
    if (d.isBLE) bleCount++;
    else wifiCount++;
  }

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setCursor(6, 42);
  M5.Display.printf("Tracked : %d", (int)devices.size());

  M5.Display.setCursor(6, 55);
  M5.Display.setTextColor(TFT_GREEN);
  M5.Display.printf("Active  : %d", active);

  M5.Display.setCursor(6, 68);
  M5.Display.setTextColor(TFT_ORANGE);
  M5.Display.printf("Ghosts  : %d", ghosts);

  // Mini bars
  int barMaxW = 70;
  int barY = 84;
  if (devices.size() > 0) {
    int actW = (active * barMaxW) / (int)devices.size();
    int ghoW = (ghosts * barMaxW) / (int)devices.size();
    M5.Display.fillRect(6, barY, actW, 6, TFT_GREEN);
    M5.Display.fillRect(6 + actW, barY, ghoW, 6, TFT_ORANGE);
    M5.Display.drawRect(6, barY, barMaxW, 6, TFT_DARKGREY);
  }

  M5.Display.setTextColor(TFT_LIGHTGREY);
  M5.Display.setCursor(6, 96);
  unsigned long age = (millis() - lastScanTime) / 1000;
  if (lastScanTime == 0) M5.Display.print("Last scan: never");
  else M5.Display.printf("Last scan: %lus", age);

  M5.Display.setCursor(6, 109);
  M5.Display.printf("BLE:%d  WiFi:%d", bleCount, wifiCount);

  // Footer
  M5.Display.fillRect(0, M5.Display.height() - 13, M5.Display.width(), 13, TFT_DARKGREY);
  M5.Display.setTextColor(TFT_YELLOW, TFT_DARKGREY);
  M5.Display.setCursor(4, M5.Display.height() - 11);
  M5.Display.print("A:Menu  B:Track");
}

void drawMenu() {
  M5.Display.fillScreen(TFT_BLACK);
  drawHeader("MENU");

  for (int i = 0; i < MENU_COUNT; i++) {
    int y = 18 + i * 13;
    if (i == menuIndex) {
      M5.Display.fillRect(0, y - 1, M5.Display.width(), 13, TFT_DARKGREEN);
      M5.Display.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    } else {
      M5.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    }
    M5.Display.setCursor(6, y);
    M5.Display.print(menuItems[i]);
  }

  M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
  M5.Display.setCursor(3, M5.Display.height() - 11);
  M5.Display.print("A:Select  B:Next");
}

void drawTrackScreen() {
  M5.Display.fillScreen(TFT_BLACK);
  drawHeader("TRACK MODE");

  M5.Display.setTextDatum(TL_DATUM);
  M5.Display.setCursor(6, 18);
  M5.Display.setTextColor(TFT_GREEN);
  M5.Display.print("Hybrid scan active");

  int y = 32;
  int shown = 0;
  // Prefer showing recent / non-left first, then ghosts
  for (int pass = 0; pass < 2 && shown < 6; pass++) {
    for (auto& d : devices) {
      if (shown >= 6) break;
      if (pass == 0 && d.left) continue;
      if (pass == 1 && !d.left) continue;

      if (d.left) {
        M5.Display.setTextColor(TFT_RED);
      } else if (d.rssi >= PROXIMITY_RSSI) {
        M5.Display.setTextColor(TFT_YELLOW);
      } else {
        M5.Display.setTextColor(TFT_WHITE);
      }
      M5.Display.setCursor(3, y);
      String shortMac = d.mac.length() > 8 ? d.mac.substring(d.mac.length() - 8) : d.mac;
      char type = d.isBLE ? 'B' : 'W';
      if (d.left) type = 'G';
      M5.Display.printf("%c %s %d", type, shortMac.c_str(), d.rssi);
      y += 12;
      shown++;
    }
  }

  if (devices.empty()) {
    M5.Display.setTextColor(TFT_DARKGREY);
    M5.Display.setCursor(6, 50);
    M5.Display.print("No devices yet");
  }

  M5.Display.setTextColor(TFT_YELLOW);
  M5.Display.setCursor(3, M5.Display.height() - 11);
  M5.Display.print("A/B: Menu");
}

void drawDeviceList(bool ghostsOnly) {
  M5.Display.fillScreen(TFT_BLACK);
  drawHeader(ghostsOnly ? "GHOSTS" : "DEVICES");

  int y = 18;
  int count = 0;
  for (auto& d : devices) {
    if (ghostsOnly && !d.left) continue;
    if (count >= 7) break;

    if (d.left) M5.Display.setTextColor(TFT_RED);
    else if (d.rssi >= PROXIMITY_RSSI) M5.Display.setTextColor(TFT_YELLOW);
    else M5.Display.setTextColor(TFT_CYAN);

    M5.Display.setCursor(3, y);
    String label = d.name.length() > 0 ? d.name : (d.vendor.length() > 0 ? d.vendor : d.mac.substring(9));
    if (label.length() > 11) label = label.substring(0, 11);
    M5.Display.printf("%s %d", label.c_str(), d.rssi);
    y += 12;
    count++;
  }

  if (count == 0) {
    M5.Display.setTextColor(TFT_DARKGREY);
    M5.Display.setCursor(6, 48);
    M5.Display.print(ghostsOnly ? "No ghosts" : "No devices");
  }

  M5.Display.setTextColor(TFT_YELLOW);
  M5.Display.setCursor(3, M5.Display.height() - 11);
  M5.Display.print("A/B: Back");
}

void drawSettings() {
  M5.Display.fillScreen(TFT_BLACK);
  drawHeader("SETTINGS");

  for (int i = 0; i < SETTINGS_COUNT; i++) {
    int y = 18 + i * 13;
    if (i == settingsIndex) {
      M5.Display.fillRect(0, y - 1, M5.Display.width(), 13, TFT_DARKGREEN);
      M5.Display.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    } else {
      M5.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    }
    M5.Display.setCursor(6, y);
    if (i == 3) {
      M5.Display.printf("%s: %s", settingsItems[i], autoSleep ? "ON" : "OFF");
    } else if (i == 4 || i == 5) {
      M5.Display.printf("%s (%d)", settingsItems[i], brightness);
    } else {
      M5.Display.print(settingsItems[i]);
    }
  }

  M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
  M5.Display.setCursor(3, M5.Display.height() - 11);
  M5.Display.print("A:Action  B:Next");
}

void drawAbout() {
  M5.Display.fillScreen(TFT_BLACK);
  drawHeader("ABOUT");

  M5.Display.setTextDatum(MC_DATUM);
  M5.Display.setTextColor(TFT_CYAN);
  M5.Display.drawString("WiFighter " VERSION_STR, M5.Display.width()/2, 28);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.drawString("BLE + WiFi Tracker", M5.Display.width()/2, 44);
  M5.Display.setTextColor(TFT_LIGHTGREY);
  M5.Display.drawString("M5StickC Plus / Plus2", M5.Display.width()/2, 60);
  M5.Display.drawString("Pulls left-device data", M5.Display.width()/2, 74);
  M5.Display.setTextColor(TFT_ORANGE);
  M5.Display.drawString("Authorized use only", M5.Display.width()/2, 90);

  M5.Display.setTextDatum(TL_DATUM);
  M5.Display.setTextColor(TFT_YELLOW);
  M5.Display.setCursor(3, M5.Display.height() - 11);
  M5.Display.print("A/B: Back");
}

// ============== SCANNING & TRACKING ==============
void startWiFiScan() {
  M5.Display.fillScreen(TFT_BLACK);
  drawHeader("WIFI SCAN");
  M5.Display.setCursor(6, 40);
  M5.Display.setTextColor(TFT_YELLOW);
  M5.Display.print("Scanning WiFi...");

  int n = WiFi.scanNetworks(false, true); // async=false, show_hidden=true

  M5.Display.fillScreen(TFT_BLACK);
  drawHeader("WIFI RESULTS");

  int y = 18;
  for (int i = 0; i < n && i < 7; ++i) {
    String ssid = WiFi.SSID(i);
    int32_t rssi = WiFi.RSSI(i);
    String bssid = WiFi.BSSIDstr(i);

    addOrUpdateDevice(bssid, ssid, rssi, false);

    M5.Display.setTextColor(rssi >= PROXIMITY_RSSI ? TFT_YELLOW : TFT_WHITE);
    M5.Display.setCursor(3, y);
    if (ssid.length() > 13) ssid = ssid.substring(0, 13);
    M5.Display.printf("%s %d", ssid.c_str(), rssi);
    y += 12;
  }

  totalSeen += n;
  lastScanTime = millis();
  WiFi.scanDelete();
  saveDevices();

  M5.Display.setTextColor(TFT_YELLOW);
  M5.Display.setCursor(3, M5.Display.height() - 11);
  M5.Display.print("A: Menu");
  playTone(1000, 40);
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
  M5.Display.setCursor(6, 40);
  M5.Display.setTextColor(TFT_YELLOW);
  M5.Display.print("Scanning BLE...");

  NimBLEScan* pScan = NimBLEDevice::getScan();
  pScan->setScanCallbacks(&bleCallbacks, false);
  pScan->setActiveScan(true);
  pScan->setInterval(90);
  pScan->setWindow(80);
  pScan->start(3, false); // 3 seconds

  delay(3400);
  pScan->stop();

  M5.Display.fillScreen(TFT_BLACK);
  drawHeader("BLE RESULTS");

  int y = 18;
  int shown = 0;
  for (auto& d : devices) {
    if (!d.isBLE) continue;
    if (shown >= 7) break;
    M5.Display.setTextColor(d.rssi >= PROXIMITY_RSSI ? TFT_YELLOW : TFT_WHITE);
    M5.Display.setCursor(3, y);
    String label = d.name.length() > 0 ? d.name : (d.vendor.length() > 0 ? d.vendor : d.mac.substring(9));
    if (label.length() > 13) label = label.substring(0, 13);
    M5.Display.printf("%s %d", label.c_str(), d.rssi);
    y += 12;
    shown++;
  }

  lastScanTime = millis();
  saveDevices();
  M5.Display.setTextColor(TFT_YELLOW);
  M5.Display.setCursor(3, M5.Display.height() - 11);
  M5.Display.print("A: Menu");
  playTone(1100, 40);
}

void updateTracking() {
  unsigned long now = millis();
  if (now - lastScanTime < 4200) return;

  // BLE burst
  NimBLEScan* pScan = NimBLEDevice::getScan();
  pScan->setScanCallbacks(&bleCallbacks, false);
  pScan->setActiveScan(true);
  pScan->start(2, false);
  delay(2200);
  pScan->stop();

  // Light WiFi
  int n = WiFi.scanNetworks(false, true, false, 220);
  for (int i = 0; i < n; ++i) {
    addOrUpdateDevice(WiFi.BSSIDstr(i), WiFi.SSID(i), WiFi.RSSI(i), false);
  }
  WiFi.scanDelete();

  // Mark left devices (ghosts)
  bool newGhost = false;
  for (auto& d : devices) {
    if (!d.left && (now - d.lastSeen > DEVICE_TIMEOUT_MS)) {
      d.left = true;
      newGhost = true;
    }
  }
  if (newGhost) {
    playTone(600, 120); // alert on new ghost
  }

  lastScanTime = now;
  saveDevices();
}

String lookupVendor(const String& mac) {
  if (mac.length() < 8) return "";
  String prefix = mac.substring(0, 8); // XX:XX:XX
  prefix.toUpperCase();
  for (int i = 0; i < OUI_COUNT; i++) {
    if (prefix.equals(ouiTable[i].prefix)) {
      return String(ouiTable[i].name);
    }
  }
  return "";
}

void addOrUpdateDevice(const String& mac, const String& name, int8_t rssi, bool isBLE) {
  for (auto& d : devices) {
    if (d.mac.equalsIgnoreCase(mac)) {
      d.rssi = rssi;
      if (rssi > d.bestRssi) d.bestRssi = rssi;
      d.lastSeen = millis();
      d.hitCount++;
      d.left = false;
      if (name.length() > 0 && (d.name.length() == 0 || d.name == d.vendor)) {
        d.name = name;
      }
      if (d.vendor.length() == 0) {
        d.vendor = lookupVendor(mac);
      }
      return;
    }
  }

  // New device
  if (devices.size() >= MAX_TRACKED_DEVICES) {
    // Prefer remove oldest ghost
    auto oldestGhost = devices.end();
    unsigned long oldest = ULONG_MAX;
    for (auto it = devices.begin(); it != devices.end(); ++it) {
      if (it->left && it->lastSeen < oldest) {
        oldest = it->lastSeen;
        oldestGhost = it;
      }
    }
    if (oldestGhost != devices.end()) {
      devices.erase(oldestGhost);
    } else {
      devices.erase(devices.begin());
    }
  }

  TrackedDevice nd;
  nd.mac = mac;
  nd.name = name;
  nd.vendor = lookupVendor(mac);
  nd.rssi = rssi;
  nd.bestRssi = rssi;
  nd.isBLE = isBLE;
  nd.lastSeen = millis();
  nd.firstSeen = millis();
  nd.hitCount = 1;
  nd.left = false;
  devices.push_back(nd);
}

void clearDevices() {
  devices.clear();
}

void clearGhosts() {
  devices.erase(
    std::remove_if(devices.begin(), devices.end(),
                   [](const TrackedDevice& d) { return d.left; }),
    devices.end());
}

void applyBrightness() {
  M5.Display.setBrightness(brightness);
}

void playTone(int freq, int duration) {
  // M5Unified Speaker (works on Plus / Plus2 when present)
  if (M5.Speaker.isEnabled()) {
    M5.Speaker.tone(freq, duration);
  }
}

// NVS persistence
void saveDevices() {
  prefs.putInt("count", (int)devices.size());
  for (size_t i = 0; i < devices.size() && i < MAX_TRACKED_DEVICES; i++) {
    String key = "d" + String(i);
    // mac|name|vendor|rssi|best|isBLE|hits|left
    String val = devices[i].mac + "|" + devices[i].name + "|" + devices[i].vendor + "|" +
                 String(devices[i].rssi) + "|" + String(devices[i].bestRssi) + "|" +
                 (devices[i].isBLE ? "1" : "0") + "|" +
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

    // Parse mac|name|vendor|rssi|best|isBLE|hits|left
    int p[7];
    int pos = 0;
    bool ok = true;
    for (int k = 0; k < 7; k++) {
      p[k] = val.indexOf('|', pos);
      if (p[k] < 0) { ok = false; break; }
      pos = p[k] + 1;
    }
    if (!ok) continue;

    TrackedDevice d;
    d.mac = val.substring(0, p[0]);
    d.name = val.substring(p[0] + 1, p[1]);
    d.vendor = val.substring(p[1] + 1, p[2]);
    d.rssi = val.substring(p[2] + 1, p[3]).toInt();
    d.bestRssi = val.substring(p[3] + 1, p[4]).toInt();
    d.isBLE = val.substring(p[4] + 1, p[5]).toInt() == 1;
    d.hitCount = val.substring(p[5] + 1, p[6]).toInt();
    d.left = val.substring(p[6] + 1).toInt() == 1;
    d.lastSeen = millis();
    d.firstSeen = millis();
    devices.push_back(d);
  }
}

void exportSerial() {
  Serial.println("=== WiFighter Device Export ===");
  Serial.printf("Count: %d  Version: %s\n", (int)devices.size(), VERSION_STR);
  for (auto& d : devices) {
    Serial.printf("%s,%s,%s,%d,%d,%s,hits=%d,left=%d\n",
                  d.mac.c_str(),
                  d.name.c_str(),
                  d.vendor.c_str(),
                  d.rssi,
                  d.bestRssi,
                  d.isBLE ? "BLE" : "WiFi",
                  d.hitCount,
                  d.left ? 1 : 0);
  }
  Serial.println("=== End ===");
}
