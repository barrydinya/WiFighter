/*
 * WiFighter v0.4 - Portable WiFi/BLE Device Data Recon Tool
 * For M5StickC Plus / M5StickC Plus2
 *
 * Collects nearby WiFi AP info, BLE advertisements, and residual device
 * identifiers (MACs, names, manufacturer data). Designed for authorized
 * security research and education only.
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
#include <algorithm>

// ==================== CONFIG ====================
#define MAX_WIFI_RESULTS   30
#define MAX_BLE_RESULTS    40
#define DEFAULT_SCAN_SEC   6
#define MENU_ITEMS         8
#define DEBOUNCE_MS        150
#define IDLE_SLEEP_MS      180000UL   // 3 min default if autoSleep enabled

// Colors
#define COL_BG         BLACK
#define COL_TITLE      TFT_CYAN
#define COL_TEXT       TFT_WHITE
#define COL_HIGHLIGHT  TFT_YELLOW
#define COL_STATUS     TFT_GREEN
#define COL_WARN       TFT_ORANGE
#define COL_ERR        TFT_RED
#define COL_DIM        TFT_DARKGREY
#define COL_BAR        TFT_NAVY
#define COL_RSSI_OK    TFT_GREEN
#define COL_RSSI_MID   TFT_YELLOW
#define COL_RSSI_LOW   TFT_ORANGE

// ==================== STATE ====================
enum AppState {
  STATE_HOME,
  STATE_MENU,
  STATE_WIFI_SCAN,
  STATE_BLE_SCAN,
  STATE_HYBRID,
  STATE_SETTINGS,
  STATE_ABOUT,
  STATE_EXPORT
};

AppState currentState = STATE_HOME;
int menuIndex = 0;
int scrollOffset = 0;
int settingsIndex = 0;
bool scanning = false;
unsigned long lastButtonTime = 0;
unsigned long lastRefresh = 0;
unsigned long lastActivity = 0;
unsigned long lastScanTime = 0;

// Runtime settings (persisted)
int scanSeconds = DEFAULT_SCAN_SEC;
int brightness = 80;
bool autoSleep = false;

// Menu labels
const char* menuItems[MENU_ITEMS] = {
  "WiFi Recon",
  "BLE Recon",
  "Hybrid Scan",
  "View Results",
  "Export Serial",
  "Settings",
  "Clear Data",
  "About"
};

// Data structures
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
void drawRssiBar(int x, int y, int rssi, int maxW = 60);
void handleButtons();
void startWifiScan();
void startBleScan();
void startHybridScan();
void exportSerial();
void clearAllData();
void loadSettings();
void saveSettings();
void updateBattery();
String authModeToStr(wifi_auth_mode_t m);
void goHome();
void goMenu();
void sortWifiByRssi();
void sortBleByRssi();
void dedupBle();
String timeSinceLastScan();

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
  delay(60);

  // NimBLE
  NimBLEDevice::init("WiFighter");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  // Persistent settings
  prefs.begin("wifighter", false);
  loadSettings();
  M5.Display.setBrightness(brightness);

  lastActivity = millis();
  updateBattery();
  drawHome();
  Serial.println("[WiFighter v0.4] Boot complete - Authorized use only");
}

// ==================== LOOP ====================
void loop() {
  M5.update();
  handleButtons();

  // Periodic refresh on home
  if (millis() - lastRefresh > 3500) {
    updateBattery();
    if (currentState == STATE_HOME) drawHome();
    lastRefresh = millis();
  }

  // Optional auto deep-sleep after idle
  if (autoSleep && !scanning && (millis() - lastActivity > IDLE_SLEEP_MS)) {
    statusMsg = "Sleep";
    drawHome();
    delay(400);
    M5.Power.deepSleep(0);   // wake on button
  }

  delay(6);
}

// ==================== NAV HELPERS ====================
void goHome() {
  currentState = STATE_HOME;
  scrollOffset = 0;
  lastActivity = millis();
  drawHome();
}

void goMenu() {
  currentState = STATE_MENU;
  menuIndex = 0;
  scrollOffset = 0;
  lastActivity = millis();
  drawMenu();
}

// ==================== BUTTON HANDLING ====================
void handleButtons() {
  if (millis() - lastButtonTime < DEBOUNCE_MS) return;

  // ---- Button A : Select / Enter / Back ----
  if (M5.BtnA.wasPressed()) {
    lastButtonTime = millis();
    lastActivity = millis();

    switch (currentState) {
      case STATE_HOME:
        goMenu();
        break;

      case STATE_MENU:
        switch (menuIndex) {
          case 0: startWifiScan(); break;
          case 1: startBleScan();  break;
          case 2: startHybridScan(); break;
          case 3: // View Results – prefer hybrid view if both, else single
            if (!wifiResults.empty() && !bleResults.empty()) {
              currentState = STATE_HYBRID;
              scrollOffset = 0;
              drawHybridResults();
            } else if (!wifiResults.empty()) {
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
            exportSerial();
            break;
          case 5:
            currentState = STATE_SETTINGS;
            settingsIndex = 0;
            drawSettings();
            break;
          case 6:
            clearAllData();
            statusMsg = "Cleared";
            drawMenu();
            break;
          case 7:
            currentState = STATE_ABOUT;
            drawAbout();
            break;
        }
        break;

      case STATE_WIFI_SCAN:
      case STATE_BLE_SCAN:
      case STATE_HYBRID:
      case STATE_ABOUT:
      case STATE_EXPORT:
        goMenu();
        break;

      case STATE_SETTINGS:
        if (settingsIndex == 0) {           // Scan seconds
          scanSeconds += 2;
          if (scanSeconds > 14) scanSeconds = 4;
        } else if (settingsIndex == 1) {    // Brightness
          brightness += 20;
          if (brightness > 100) brightness = 40;
          M5.Display.setBrightness(brightness);
        } else if (settingsIndex == 2) {    // Auto-sleep toggle
          autoSleep = !autoSleep;
        } else if (settingsIndex == 3) {    // Save & Exit
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
    lastActivity = millis();

    if (currentState == STATE_MENU) {
      menuIndex = (menuIndex + 1) % MENU_ITEMS;
      drawMenu();
    }
    else if (currentState == STATE_SETTINGS) {
      settingsIndex = (settingsIndex + 1) % 4;
      drawSettings();
    }
    else if (currentState == STATE_WIFI_SCAN) {
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
      int total = (int)wifiResults.size() + (int)bleResults.size();
      if (total > 0) {
        scrollOffset = (scrollOffset + 1) % total;
        drawHybridResults();
      }
    }
  }

  // Power button short = Home
  if (M5.BtnPWR.wasClicked()) {
    lastButtonTime = millis();
    lastActivity = millis();
    goHome();
  }
}

// ==================== DRAWING ====================
void drawStatusBar() {
  M5.Display.fillRect(0, 0, M5.Display.width(), 13, COL_BAR);
  M5.Display.setTextColor(COL_TEXT);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(2, 2);
  M5.Display.printf("WiFighter %d%%", batteryPct);
  M5.Display.setCursor(M5.Display.width() - 52, 2);
  M5.Display.print(statusMsg.substring(0, 8));
}

void drawRssiBar(int x, int y, int rssi, int maxW) {
  // Map -90..-30 dBm to 0..maxW
  int level = constrain(map(rssi, -90, -30, 0, maxW), 0, maxW);
  uint16_t col = COL_RSSI_LOW;
  if (rssi > -55) col = COL_RSSI_OK;
  else if (rssi > -70) col = COL_RSSI_MID;

  M5.Display.drawRect(x, y, maxW + 2, 8, COL_DIM);
  if (level > 0) {
    M5.Display.fillRect(x + 1, y + 1, level, 6, col);
  }
}

void drawHome() {
  M5.Display.fillScreen(COL_BG);
  drawStatusBar();

  // Title block
  M5.Display.setTextColor(COL_TITLE);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(14, 20);
  M5.Display.print("WiFighter");

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(COL_DIM);
  M5.Display.setCursor(18, 40);
  M5.Display.print("BLE + WiFi Recon v0.4");

  // Status
  M5.Display.setTextColor(COL_STATUS);
  M5.Display.setCursor(8, 58);
  M5.Display.print("Status: ");
  M5.Display.setTextColor(COL_TEXT);
  M5.Display.print(statusMsg);

  // Counts with simple visual
  M5.Display.setTextColor(COL_TEXT);
  M5.Display.setCursor(8, 76);
  M5.Display.printf("WiFi  : %2d", (int)wifiResults.size());
  M5.Display.setCursor(8, 90);
  M5.Display.printf("BLE   : %2d", (int)bleResults.size());

  // Last scan age
  M5.Display.setTextColor(COL_DIM);
  M5.Display.setCursor(8, 106);
  M5.Display.print(timeSinceLastScan());

  // Hint
  M5.Display.setTextColor(COL_HIGHLIGHT);
  M5.Display.setCursor(8, 122);
  M5.Display.print("A:Menu   PWR:Home");
}

void drawMenu() {
  M5.Display.fillScreen(COL_BG);
  drawStatusBar();

  M5.Display.setTextColor(COL_TITLE);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(6, 16);
  M5.Display.print("== MAIN MENU ==");

  // Show 7 items max on screen; scroll if needed (we have 8)
  int start = 0;
  if (menuIndex > 5) start = menuIndex - 5;

  for (int i = 0; i < 6 && (start + i) < MENU_ITEMS; i++) {
    int idx = start + i;
    int y = 30 + i * 13;
    if (idx == menuIndex) {
      M5.Display.fillRect(2, y - 1, M5.Display.width() - 4, 13, TFT_DARKGREY);
      M5.Display.setTextColor(COL_HIGHLIGHT);
      M5.Display.setCursor(6, y);
      M5.Display.print("> ");
      M5.Display.print(menuItems[idx]);
    } else {
      M5.Display.setTextColor(COL_TEXT);
      M5.Display.setCursor(14, y);
      M5.Display.print(menuItems[idx]);
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
    M5.Display.printf("SSID: %.16s", e.ssid.c_str());
    M5.Display.setCursor(4, 43);
    M5.Display.printf("BSSID:%s", e.bssid.c_str());
    M5.Display.setCursor(4, 56);
    M5.Display.printf("RSSI: %d dBm  Ch:%d", e.rssi, e.channel);
    drawRssiBar(4, 70, e.rssi, 70);
    M5.Display.setCursor(4, 82);
    M5.Display.printf("Enc : %s", authModeToStr(e.enc).c_str());

    // Peek next
    M5.Display.setTextColor(COL_DIM);
    if (wifiResults.size() > 1) {
      size_t idx = (scrollOffset + 1) % wifiResults.size();
      M5.Display.setCursor(4, 100);
      M5.Display.printf("Next: %.12s %ddB",
                        wifiResults[idx].ssid.c_str(),
                        wifiResults[idx].rssi);
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
    M5.Display.printf("Name: %.15s", e.name.c_str());
    M5.Display.setCursor(4, 43);
    M5.Display.printf("MAC : %s", e.address.c_str());
    M5.Display.setCursor(4, 56);
    M5.Display.printf("RSSI: %d dBm", e.rssi);
    drawRssiBar(4, 70, e.rssi, 70);
    if (e.manufacturer.length() > 0) {
      M5.Display.setCursor(4, 82);
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

  int total = (int)wifiResults.size() + (int)bleResults.size();
  if (total == 0) {
    M5.Display.setTextColor(COL_WARN);
    M5.Display.setCursor(8, 50);
    M5.Display.print("No data yet.");
  } else {
    int idx = scrollOffset % total;
    if (idx < (int)wifiResults.size()) {
      const WifiEntry& e = wifiResults[idx];
      M5.Display.setTextColor(COL_STATUS);
      M5.Display.setCursor(4, 32);
      M5.Display.print("[WiFi]");
      M5.Display.setTextColor(COL_TEXT);
      M5.Display.setCursor(4, 46);
      M5.Display.printf("%.16s", e.ssid.c_str());
      M5.Display.setCursor(4, 60);
      M5.Display.printf("%s  %ddB", e.bssid.c_str(), e.rssi);
      drawRssiBar(4, 74, e.rssi, 70);
      M5.Display.setCursor(4, 86);
      M5.Display.printf("Ch:%d  %s", e.channel, authModeToStr(e.enc).c_str());
    } else {
      int bIdx = idx - (int)wifiResults.size();
      const BleEntry& e = bleResults[bIdx];
      M5.Display.setTextColor(COL_STATUS);
      M5.Display.setCursor(4, 32);
      M5.Display.print("[BLE]");
      M5.Display.setTextColor(COL_TEXT);
      M5.Display.setCursor(4, 46);
      M5.Display.printf("%.16s", e.name.c_str());
      M5.Display.setCursor(4, 60);
      M5.Display.printf("%s", e.address.c_str());
      drawRssiBar(4, 74, e.rssi, 70);
      M5.Display.setCursor(4, 86);
      M5.Display.printf("%d dBm  %s", e.rssi, e.manufacturer.c_str());
    }
    M5.Display.setTextColor(COL_DIM);
    M5.Display.setCursor(4, 104);
    M5.Display.printf("Item %d of %d", idx + 1, total);
  }

  M5.Display.setTextColor(COL_HIGHLIGHT);
  M5.Display.setCursor(4, 122);
  M5.Display.print("B:Next  A:Back");
}

void drawSettings() {
  M5.Display.fillScreen(COL_BG);
  drawStatusBar();

  M5.Display.setTextColor(COL_TITLE);
  M5.Display.setCursor(6, 16);
  M5.Display.print("== SETTINGS ==");

  auto drawItem = [&](int idx, int y, const char* label, const String& value) {
    if (settingsIndex == idx) {
      M5.Display.fillRect(2, y - 1, M5.Display.width() - 4, 12, TFT_DARKGREY);
      M5.Display.setTextColor(COL_HIGHLIGHT);
    } else {
      M5.Display.setTextColor(COL_TEXT);
    }
    M5.Display.setCursor(6, y);
    M5.Display.printf("%s %s", label, value.c_str());
  };

  drawItem(0, 32, "Scan time:", String(scanSeconds) + "s");
  drawItem(1, 46, "Brightness:", String(brightness));
  drawItem(2, 60, "Auto-sleep:", autoSleep ? "ON" : "OFF");
  drawItem(3, 74, "Save & Exit", "");

  M5.Display.setTextColor(COL_DIM);
  M5.Display.setCursor(6, 96);
  M5.Display.print("A:Change  B:Next");
  M5.Display.setCursor(6, 108);
  M5.Display.print("Sleep after 3 min idle");

  M5.Display.setTextColor(COL_WARN);
  M5.Display.setCursor(6, 122);
  M5.Display.print("Persists in NVS");
}

void drawAbout() {
  M5.Display.fillScreen(COL_BG);
  drawStatusBar();

  M5.Display.setTextColor(COL_TITLE);
  M5.Display.setCursor(6, 18);
  M5.Display.print("WiFighter v0.4");

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
void sortWifiByRssi() {
  std::sort(wifiResults.begin(), wifiResults.end(),
            [](const WifiEntry& a, const WifiEntry& b) { return a.rssi > b.rssi; });
}

void sortBleByRssi() {
  std::sort(bleResults.begin(), bleResults.end(),
            [](const BleEntry& a, const BleEntry& b) { return a.rssi > b.rssi; });
}

void dedupBle() {
  // Keep strongest RSSI per unique MAC
  std::vector<BleEntry> unique;
  for (const auto& e : bleResults) {
    bool found = false;
    for (auto& u : unique) {
      if (u.address == e.address) {
        if (e.rssi > u.rssi) u = e;
        found = true;
        break;
      }
    }
    if (!found) unique.push_back(e);
  }
  bleResults = unique;
}

void startWifiScan() {
  statusMsg = "WiFi...";
  scanning = true;
  lastActivity = millis();
  drawHome();
  delay(20);

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
  sortWifiByRssi();

  lastScanTime = millis();
  statusMsg = "WiFi OK";
  scanning = false;
  currentState = STATE_WIFI_SCAN;
  drawWifiResults();
  Serial.printf("[WiFi] %d networks\n", wifiResults.size());
}

void startBleScan() {
  statusMsg = "BLE...";
  scanning = true;
  lastActivity = millis();
  drawHome();
  delay(20);

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
  dedupBle();
  sortBleByRssi();

  lastScanTime = millis();
  statusMsg = "BLE OK";
  scanning = false;
  currentState = STATE_BLE_SCAN;
  drawBleResults();
  Serial.printf("[BLE] %d devices (deduped)\n", bleResults.size());
}

void startHybridScan() {
  statusMsg = "Hybrid...";
  scanning = true;
  lastActivity = millis();
  drawHome();
  delay(15);

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
  sortWifiByRssi();

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
  dedupBle();
  sortBleByRssi();

  lastScanTime = millis();
  statusMsg = "Done";
  scanning = false;
  currentState = STATE_HYBRID;
  scrollOffset = 0;
  drawHybridResults();
  Serial.printf("[Hybrid] WiFi:%d  BLE:%d\n", wifiResults.size(), bleResults.size());
}

void exportSerial() {
  statusMsg = "Export";
  currentState = STATE_EXPORT;
  drawHome();

  Serial.println("========== WiFighter Export ==========");
  Serial.printf("WiFi networks: %d\n", wifiResults.size());
  for (size_t i = 0; i < wifiResults.size(); i++) {
    const auto& e = wifiResults[i];
    Serial.printf("%2d | %-20s | %s | %4d dBm | Ch %2d | %s\n",
                  (int)i + 1,
                  e.ssid.c_str(),
                  e.bssid.c_str(),
                  e.rssi,
                  e.channel,
                  authModeToStr(e.enc).c_str());
  }
  Serial.printf("\nBLE devices: %d\n", bleResults.size());
  for (size_t i = 0; i < bleResults.size(); i++) {
    const auto& e = bleResults[i];
    Serial.printf("%2d | %-16s | %s | %4d dBm | Mfr %s\n",
                  (int)i + 1,
                  e.name.c_str(),
                  e.address.c_str(),
                  e.rssi,
                  e.manufacturer.c_str());
  }
  Serial.println("======================================");

  statusMsg = "Exported";
  delay(400);
  goMenu();
}

void clearAllData() {
  wifiResults.clear();
  bleResults.clear();
  scrollOffset = 0;
  lastScanTime = 0;
  statusMsg = "Cleared";
  Serial.println("[Clear] All results wiped");
}

// ==================== PERSISTENCE ====================
void loadSettings() {
  scanSeconds = prefs.getInt("scanSec", DEFAULT_SCAN_SEC);
  brightness  = prefs.getInt("bright", 80);
  autoSleep   = prefs.getBool("autoSleep", false);
  if (scanSeconds < 4 || scanSeconds > 14) scanSeconds = DEFAULT_SCAN_SEC;
  if (brightness < 40 || brightness > 100) brightness = 80;
}

void saveSettings() {
  prefs.putInt("scanSec", scanSeconds);
  prefs.putInt("bright", brightness);
  prefs.putBool("autoSleep", autoSleep);
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

String timeSinceLastScan() {
  if (lastScanTime == 0) return "Last scan: never";
  unsigned long sec = (millis() - lastScanTime) / 1000;
  if (sec < 60) return "Last scan: " + String(sec) + "s ago";
  if (sec < 3600) return "Last scan: " + String(sec / 60) + "m ago";
  return "Last scan: " + String(sec / 3600) + "h ago";
}
