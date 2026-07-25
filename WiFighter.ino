/*
 * WiFighter v0.2 - Portable WiFi/BLE Device Data Recon Tool
 * For M5StickC Plus / M5StickC Plus2
 *
 * Collects nearby WiFi AP info, BLE advertisements, and residual device
 * identifiers (MACs, names, preferred networks via probes where possible).
 * Designed for authorized security research and education only.
 *
 * Hardware: M5StickC Plus or Plus2 (Plus2 recommended for RF)
 * Libraries: M5Unified, M5GFX, NimBLE-Arduino
 *
 * Controls:
 *   Button A (front M5): Select / Enter / Back (context)
 *   Button B (side):     Next item / Scroll
 *   Power button short:  Home (Plus2 / supported boards)
 *
 * Author: barrydinya + Grok collaboration
 * License: MIT (educational / authorized use only)
 */

#include <M5Unified.h>
#include <WiFi.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <vector>
#include <string>

// ==================== CONFIG ====================
#define MAX_WIFI_RESULTS   25
#define MAX_BLE_RESULTS    35
#define DEFAULT_SCAN_SEC   6
#define MENU_ITEMS         7
#define DEBOUNCE_MS        160

// Colors
#define COL_BG         BLACK
#define COL_TITLE      TFT_CYAN
#define COL_TEXT       TFT_WHITE
#define COL_HIGHLIGHT  TFT_YELLOW
#define COL_STATUS     TFT_GREEN
#define COL_WARN       TFT_ORANGE
#define COL_ERR        TFT_RED
#define COL_DIM        TFT_DARKGREY

// ==================== STATE ====================
enum AppState {
  STATE_HOME,
  STATE_MENU,
  STATE_WIFI_SCAN,
  STATE_BLE_SCAN,
  STATE_HYBRID,
  STATE_LOGS,
  STATE_SETTINGS,
  STATE_ABOUT
};

AppState currentState = STATE_HOME;
int menuIndex = 0;
int scrollOffset = 0;
int settingsIndex = 0;
bool scanning = false;
unsigned long lastButtonTime = 0;
unsigned long lastRefresh = 0;

// Runtime settings (persisted)
int scanSeconds = DEFAULT_SCAN_SEC;
int brightness = 80;
bool autoSleep = false;

// Menu
const char* menuItems[MENU_ITEMS] = {
  "WiFi Recon",
  "BLE Recon",
  "Hybrid Scan",
  "View Results",
  "Settings",
  "Clear Data",
  "About"
};

// Data
struct WifiEntry {
  String ssid;
  String bssid;
  int32_t rssi;
  uint8_t channel;
  wifi_auth_mode_t enc;
};

struct BleEntry {
  String name;
  String address;
  int rssi;
  String manufacturer;
};

std::vector<WifiEntry> wifiResults;
std::vector<BleEntry> bleResults;
String statusMsg = "Ready";
int batteryPct = 100;
Preferences prefs;

// ==================== FORWARD DECLS ====================
void drawHome();
void drawMenu();
void drawWifiResults();
void drawBleResults();
void drawHybridResults();
void drawSettings();
void drawAbout();
void drawStatusBar();
void handleButtons();
void startWifiScan();
void startBleScan();
void startHybridScan();
void clearAllData();
void loadSettings();
void saveSettings();
void updateBattery();
String authModeToStr(wifi_auth_mode_t m);
void goHome();
void goMenu();

// ==================== SETUP ====================
void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);
  M5.Display.setRotation(1);          // Landscape
  M5.Display.setBrightness(brightness);
  M5.Display.fillScreen(COL_BG);
  M5.Display.setTextDatum(top_left);

  // WiFi station for scanning
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(80);

  // NimBLE
  NimBLEDevice::init("WiFighter");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  // Persistent settings
  prefs.begin("wifighter", false);
  loadSettings();
  M5.Display.setBrightness(brightness);

  updateBattery();
  drawHome();
  Serial.println("[WiFighter v0.2] Boot complete - Authorized use only");
}

// ==================== LOOP ====================
void loop() {
  M5.update();
  handleButtons();

  // Light refresh on home
  if (millis() - lastRefresh > 4000) {
    updateBattery();
    if (currentState == STATE_HOME) drawHome();
    lastRefresh = millis();
  }

  delay(8);
}

// ==================== NAV HELPERS ====================
void goHome() {
  currentState = STATE_HOME;
  scrollOffset = 0;
  drawHome();
}

void goMenu() {
  currentState = STATE_MENU;
  menuIndex = 0;
  scrollOffset = 0;
  drawMenu();
}

// ==================== BUTTON HANDLING ====================
void handleButtons() {
  if (millis() - lastButtonTime < DEBOUNCE_MS) return;

  // ---- Button A : Select / Enter / Back ----
  if (M5.BtnA.wasPressed()) {
    lastButtonTime = millis();

    switch (currentState) {
      case STATE_HOME:
        goMenu();
        break;

      case STATE_MENU:
        switch (menuIndex) {
          case 0: startWifiScan(); break;
          case 1: startBleScan();  break;
          case 2: startHybridScan(); break;
          case 3: // View Results
            if (!wifiResults.empty()) {
              currentState = STATE_WIFI_SCAN;
              scrollOffset = 0;
              drawWifiResults();
            } else if (!bleResults.empty()) {
              currentState = STATE_BLE_SCAN;
              scrollOffset = 0;
              drawBleResults();
            } else {
              statusMsg = "No data";
              drawMenu();
            }
            break;
          case 4:
            currentState = STATE_SETTINGS;
            settingsIndex = 0;
            drawSettings();
            break;
          case 5:
            clearAllData();
            statusMsg = "Cleared";
            drawMenu();
            break;
          case 6:
            currentState = STATE_ABOUT;
            drawAbout();
            break;
        }
        break;

      case STATE_WIFI_SCAN:
      case STATE_BLE_SCAN:
      case STATE_HYBRID:
      case STATE_LOGS:
      case STATE_ABOUT:
        goMenu();   // A = Back to menu from results/about
        break;

      case STATE_SETTINGS:
        // Cycle value or save
        if (settingsIndex == 0) {           // Scan seconds
          scanSeconds += 2;
          if (scanSeconds > 14) scanSeconds = 4;
        } else if (settingsIndex == 1) {    // Brightness
          brightness += 20;
          if (brightness > 100) brightness = 40;
          M5.Display.setBrightness(brightness);
        } else if (settingsIndex == 2) {    // Save & exit
          saveSettings();
          statusMsg = "Saved";
          goMenu();
          return;
        }
        drawSettings();
        break;

      default: break;
    }
  }

  // ---- Button B : Next / Scroll ----
  if (M5.BtnB.wasPressed()) {
    lastButtonTime = millis();

    if (currentState == STATE_MENU) {
      menuIndex = (menuIndex + 1) % MENU_ITEMS;
      drawMenu();
    }
    else if (currentState == STATE_SETTINGS) {
      settingsIndex = (settingsIndex + 1) % 3;
      drawSettings();
    }
    else if (currentState == STATE_WIFI_SCAN || currentState == STATE_LOGS) {
      if (!wifiResults.empty()) {
        scrollOffset = (scrollOffset + 1) % wifiResults.size();
        drawWifiResults();
      }
    }
    else if (currentState == STATE_BLE_SCAN) {
      if (!bleResults.empty()) {
        scrollOffset = (scrollOffset + 1) % bleResults.size();
        drawBleResults();
      }
    }
    else if (currentState == STATE_HYBRID) {
      // Alternate view or just scroll wifi then ble conceptually
      scrollOffset = (scrollOffset + 1) % max(1, (int)(wifiResults.size() + bleResults.size()));
      drawHybridResults();
    }
  }

  // Power button short = Home (supported on many boards)
  if (M5.BtnPWR.wasClicked()) {
    lastButtonTime = millis();
    goHome();
  }
}

// ==================== DRAWING ====================
void drawStatusBar() {
  M5.Display.fillRect(0, 0, M5.Display.width(), 13, TFT_NAVY);
  M5.Display.setTextColor(COL_TEXT);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(2, 2);
  M5.Display.printf("WiFighter  %d%%", batteryPct);
  M5.Display.setCursor(M5.Display.width() - 48, 2);
  M5.Display.print(statusMsg.substring(0, 7));
}

void drawHome() {
  M5.Display.fillScreen(COL_BG);
  drawStatusBar();

  M5.Display.setTextColor(COL_TITLE);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(14, 26);
  M5.Display.print("WiFighter");

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(COL_TEXT);
  M5.Display.setCursor(10, 50);
  M5.Display.print("BLE + WiFi Device Recon");

  M5.Display.setCursor(10, 68);
  M5.Display.setTextColor(COL_STATUS);
  M5.Display.print("Status: ");
  M5.Display.print(statusMsg);

  M5.Display.setTextColor(COL_TEXT);
  M5.Display.setCursor(10, 88);
  M5.Display.printf("WiFi : %d nets", wifiResults.size());
  M5.Display.setCursor(10, 101);
  M5.Display.printf("BLE  : %d devs", bleResults.size());

  M5.Display.setTextColor(COL_HIGHLIGHT);
  M5.Display.setCursor(10, 122);
  M5.Display.print("A:Menu   B:Scroll");
}

void drawMenu() {
  M5.Display.fillScreen(COL_BG);
  drawStatusBar();

  M5.Display.setTextColor(COL_TITLE);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(6, 16);
  M5.Display.print("== MAIN MENU ==");

  for (int i = 0; i < MENU_ITEMS; i++) {
    int y = 30 + i * 13;
    if (i == menuIndex) {
      M5.Display.fillRect(3, y - 1, M5.Display.width() - 6, 12, TFT_DARKGREY);
      M5.Display.setTextColor(COL_HIGHLIGHT);
      M5.Display.setCursor(8, y);
      M5.Display.print("> ");
      M5.Display.print(menuItems[i]);
    } else {
      M5.Display.setTextColor(COL_TEXT);
      M5.Display.setCursor(16, y);
      M5.Display.print(menuItems[i]);
    }
  }

  M5.Display.setTextColor(COL_WARN);
  M5.Display.setCursor(6, 122);
  M5.Display.print("A:Select  B:Next");
}

void drawWifiResults() {
  M5.Display.fillScreen(COL_BG);
  drawStatusBar();

  M5.Display.setTextColor(COL_TITLE);
  M5.Display.setCursor(4, 15);
  M5.Display.printf("WiFi %d/%d", scrollOffset + 1, (int)wifiResults.size());

  if (wifiResults.empty()) {
    M5.Display.setTextColor(COL_WARN);
    M5.Display.setCursor(8, 50);
    M5.Display.print("No results.");
    M5.Display.setCursor(8, 64);
    M5.Display.print("Run a scan first.");
  } else {
    const WifiEntry& e = wifiResults[scrollOffset % wifiResults.size()];
    M5.Display.setTextColor(COL_TEXT);
    M5.Display.setCursor(4, 30);
    M5.Display.printf("SSID: %.15s", e.ssid.c_str());
    M5.Display.setCursor(4, 43);
    M5.Display.printf("BSSID:%s", e.bssid.c_str());
    M5.Display.setCursor(4, 56);
    M5.Display.printf("RSSI: %d dBm  Ch:%d", e.rssi, e.channel);
    M5.Display.setCursor(4, 69);
    M5.Display.printf("Enc : %s", authModeToStr(e.enc).c_str());

    // Peek next entries
    M5.Display.setTextColor(COL_DIM);
    int shown = 0;
    for (size_t i = 1; i < wifiResults.size() && shown < 2; i++) {
      size_t idx = (scrollOffset + i) % wifiResults.size();
      M5.Display.setCursor(4, 90 + shown * 12);
      M5.Display.printf("%s %ddB",
                        wifiResults[idx].ssid.substring(0, 11).c_str(),
                        wifiResults[idx].rssi);
      shown++;
    }
  }

  M5.Display.setTextColor(COL_HIGHLIGHT);
  M5.Display.setCursor(4, 122);
  M5.Display.print("B:Next  A:Back");
}

void drawBleResults() {
  M5.Display.fillScreen(COL_BG);
  drawStatusBar();

  M5.Display.setTextColor(COL_TITLE);
  M5.Display.setCursor(4, 15);
  M5.Display.printf("BLE %d/%d", scrollOffset + 1, (int)bleResults.size());

  if (bleResults.empty()) {
    M5.Display.setTextColor(COL_WARN);
    M5.Display.setCursor(8, 50);
    M5.Display.print("No BLE devices.");
  } else {
    const BleEntry& e = bleResults[scrollOffset % bleResults.size()];
    M5.Display.setTextColor(COL_TEXT);
    M5.Display.setCursor(4, 30);
    M5.Display.printf("Name: %.14s", e.name.c_str());
    M5.Display.setCursor(4, 43);
    M5.Display.printf("MAC : %s", e.address.c_str());
    M5.Display.setCursor(4, 56);
    M5.Display.printf("RSSI: %d dBm", e.rssi);
    if (e.manufacturer.length() > 0) {
      M5.Display.setCursor(4, 69);
      M5.Display.printf("Mfr : %.16s", e.manufacturer.c_str());
    }
  }

  M5.Display.setTextColor(COL_HIGHLIGHT);
  M5.Display.setCursor(4, 122);
  M5.Display.print("B:Next  A:Back");
}

void drawHybridResults() {
  M5.Display.fillScreen(COL_BG);
  drawStatusBar();

  M5.Display.setTextColor(COL_TITLE);
  M5.Display.setCursor(4, 15);
  M5.Display.printf("Hybrid  W:%d B:%d", (int)wifiResults.size(), (int)bleResults.size());

  M5.Display.setTextColor(COL_TEXT);
  M5.Display.setCursor(4, 32);
  M5.Display.print("Top WiFi:");
  if (!wifiResults.empty()) {
    M5.Display.setCursor(4, 44);
    M5.Display.printf("%.14s %ddB", wifiResults[0].ssid.c_str(), wifiResults[0].rssi);
  } else {
    M5.Display.setCursor(4, 44);
    M5.Display.print("(none)");
  }

  M5.Display.setCursor(4, 62);
  M5.Display.print("Top BLE:");
  if (!bleResults.empty()) {
    M5.Display.setCursor(4, 74);
    M5.Display.printf("%.14s %ddB", bleResults[0].name.c_str(), bleResults[0].rssi);
  } else {
    M5.Display.setCursor(4, 74);
    M5.Display.print("(none)");
  }

  M5.Display.setTextColor(COL_DIM);
  M5.Display.setCursor(4, 96);
  M5.Display.print("Use View Results for detail");

  M5.Display.setTextColor(COL_HIGHLIGHT);
  M5.Display.setCursor(4, 122);
  M5.Display.print("A:Back to Menu");
}

void drawSettings() {
  M5.Display.fillScreen(COL_BG);
  drawStatusBar();

  M5.Display.setTextColor(COL_TITLE);
  M5.Display.setCursor(6, 16);
  M5.Display.print("== SETTINGS ==");

  // Item 0 - Scan duration
  int y = 34;
  if (settingsIndex == 0) {
    M5.Display.fillRect(3, y - 1, M5.Display.width() - 6, 12, TFT_DARKGREY);
    M5.Display.setTextColor(COL_HIGHLIGHT);
  } else {
    M5.Display.setTextColor(COL_TEXT);
  }
  M5.Display.setCursor(8, y);
  M5.Display.printf("Scan time: %ds", scanSeconds);

  // Item 1 - Brightness
  y = 48;
  if (settingsIndex == 1) {
    M5.Display.fillRect(3, y - 1, M5.Display.width() - 6, 12, TFT_DARKGREY);
    M5.Display.setTextColor(COL_HIGHLIGHT);
  } else {
    M5.Display.setTextColor(COL_TEXT);
  }
  M5.Display.setCursor(8, y);
  M5.Display.printf("Brightness: %d", brightness);

  // Item 2 - Save
  y = 62;
  if (settingsIndex == 2) {
    M5.Display.fillRect(3, y - 1, M5.Display.width() - 6, 12, TFT_DARKGREY);
    M5.Display.setTextColor(COL_HIGHLIGHT);
  } else {
    M5.Display.setTextColor(COL_TEXT);
  }
  M5.Display.setCursor(8, y);
  M5.Display.print("Save & Exit");

  M5.Display.setTextColor(COL_DIM);
  M5.Display.setCursor(6, 90);
  M5.Display.print("A:Change/Select");
  M5.Display.setCursor(6, 102);
  M5.Display.print("B:Next item");

  M5.Display.setTextColor(COL_WARN);
  M5.Display.setCursor(6, 122);
  M5.Display.print("Changes persist");
}

void drawAbout() {
  M5.Display.fillScreen(COL_BG);
  drawStatusBar();

  M5.Display.setTextColor(COL_TITLE);
  M5.Display.setCursor(6, 18);
  M5.Display.print("WiFighter v0.2");

  M5.Display.setTextColor(COL_TEXT);
  M5.Display.setCursor(6, 36);
  M5.Display.print("Authorized research");
  M5.Display.setCursor(6, 48);
  M5.Display.print("only. No illegal use.");

  M5.Display.setCursor(6, 66);
  M5.Display.print("M5StickC Plus/Plus2");
  M5.Display.setCursor(6, 78);
  M5.Display.print("Arduino + NimBLE");

  M5.Display.setCursor(6, 96);
  M5.Display.print("github.com/barrydinya");

  M5.Display.setTextColor(COL_HIGHLIGHT);
  M5.Display.setCursor(6, 122);
  M5.Display.print("A:Back  PWR:Home");
}

// ==================== SCANS ====================
void startWifiScan() {
  statusMsg = "WiFi...";
  scanning = true;
  drawHome();
  delay(30);

  wifiResults.clear();
  scrollOffset = 0;

  int n = WiFi.scanNetworks(false, true); // blocking, show hidden
  if (n < 0) n = 0;
  if (n > MAX_WIFI_RESULTS) n = MAX_WIFI_RESULTS;

  for (int i = 0; i < n; i++) {
    WifiEntry e;
    e.ssid    = WiFi.SSID(i);
    e.bssid   = WiFi.BSSIDstr(i);
    e.rssi    = WiFi.RSSI(i);
    e.channel = WiFi.channel(i);
    e.enc     = WiFi.encryptionType(i);
    wifiResults.push_back(e);
  }
  WiFi.scanDelete();

  statusMsg = "WiFi OK";
  scanning = false;
  currentState = STATE_WIFI_SCAN;
  drawWifiResults();
  Serial.printf("[WiFi] %d networks\n", wifiResults.size());
}

void startBleScan() {
  statusMsg = "BLE...";
  scanning = true;
  drawHome();
  delay(30);

  bleResults.clear();
  scrollOffset = 0;

  NimBLEScan* pScan = NimBLEDevice::getScan();
  pScan->setActiveScan(true);
  pScan->setInterval(45);
  pScan->setWindow(30);
  pScan->setMaxResults(0);

  NimBLEScanResults results = pScan->start(scanSeconds, false);

  int count = results.getCount();
  for (int i = 0; i < count && (int)bleResults.size() < MAX_BLE_RESULTS; i++) {
    const NimBLEAdvertisedDevice* dev = results.getDevice(i);
    BleEntry e;
    e.address = dev->getAddress().toString().c_str();
    e.rssi    = dev->getRSSI();
    e.name    = dev->haveName() ? String(dev->getName().c_str()) : "(unknown)";
    e.manufacturer = "";
    if (dev->haveManufacturerData()) {
      std::string md = dev->getManufacturerData();
      if (md.size() >= 2) {
        char buf[20];
        snprintf(buf, sizeof(buf), "%02X%02X", (uint8_t)md[0], (uint8_t)md[1]);
        e.manufacturer = buf;
      }
    }
    bleResults.push_back(e);
  }
  pScan->clearResults();

  statusMsg = "BLE OK";
  scanning = false;
  currentState = STATE_BLE_SCAN;
  drawBleResults();
  Serial.printf("[BLE] %d devices\n", bleResults.size());
}

void startHybridScan() {
  statusMsg = "Hybrid...";
  scanning = true;
  drawHome();
  delay(20);

  // WiFi first
  wifiResults.clear();
  int n = WiFi.scanNetworks(false, true);
  if (n < 0) n = 0;
  if (n > MAX_WIFI_RESULTS) n = MAX_WIFI_RESULTS;
  for (int i = 0; i < n; i++) {
    WifiEntry e;
    e.ssid    = WiFi.SSID(i);
    e.bssid   = WiFi.BSSIDstr(i);
    e.rssi    = WiFi.RSSI(i);
    e.channel = WiFi.channel(i);
    e.enc     = WiFi.encryptionType(i);
    wifiResults.push_back(e);
  }
  WiFi.scanDelete();

  // Then BLE
  bleResults.clear();
  NimBLEScan* pScan = NimBLEDevice::getScan();
  pScan->setActiveScan(true);
  pScan->setInterval(45);
  pScan->setWindow(30);
  pScan->setMaxResults(0);
  NimBLEScanResults results = pScan->start(scanSeconds, false);
  int count = results.getCount();
  for (int i = 0; i < count && (int)bleResults.size() < MAX_BLE_RESULTS; i++) {
    const NimBLEAdvertisedDevice* dev = results.getDevice(i);
    BleEntry e;
    e.address = dev->getAddress().toString().c_str();
    e.rssi    = dev->getRSSI();
    e.name    = dev->haveName() ? String(dev->getName().c_str()) : "(unknown)";
    e.manufacturer = "";
    if (dev->haveManufacturerData()) {
      std::string md = dev->getManufacturerData();
      if (md.size() >= 2) {
        char buf[20];
        snprintf(buf, sizeof(buf), "%02X%02X", (uint8_t)md[0], (uint8_t)md[1]);
        e.manufacturer = buf;
      }
    }
    bleResults.push_back(e);
  }
  pScan->clearResults();

  statusMsg = "Done";
  scanning = false;
  currentState = STATE_HYBRID;
  scrollOffset = 0;
  drawHybridResults();
  Serial.printf("[Hybrid] WiFi:%d  BLE:%d\n", wifiResults.size(), bleResults.size());
}

void clearAllData() {
  wifiResults.clear();
  bleResults.clear();
  scrollOffset = 0;
  statusMsg = "Cleared";
  Serial.println("[Clear] All results wiped");
}

// ==================== PERSISTENCE ====================
void loadSettings() {
  scanSeconds = prefs.getInt("scanSec", DEFAULT_SCAN_SEC);
  brightness  = prefs.getInt("bright", 80);
  if (scanSeconds < 4 || scanSeconds > 14) scanSeconds = DEFAULT_SCAN_SEC;
  if (brightness < 40 || brightness > 100) brightness = 80;
}

void saveSettings() {
  prefs.putInt("scanSec", scanSeconds);
  prefs.putInt("bright", brightness);
  Serial.println("[Settings] Saved to NVS");
}

// ==================== HELPERS ====================
void updateBattery() {
  batteryPct = M5.Power.getBatteryLevel();
  if (batteryPct < 0) batteryPct = 0;
  if (batteryPct > 100) batteryPct = 100;
}

String authModeToStr(wifi_auth_mode_t m) {
  switch (m) {
    case WIFI_AUTH_OPEN:            return "OPEN";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENT";
    case WIFI_AUTH_WPA3_PSK:        return "WPA3";
    default:                        return "UNK";
  }
}
