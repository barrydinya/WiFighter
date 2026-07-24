/*
 * WiFighter - Portable WiFi/BLE Device Data Recon Tool
 * For M5StickC Plus / M5StickC Plus2
 * 
 * Collects nearby WiFi AP info, BLE advertisements, and residual device
 * identifiers (MACs, names, preferred networks via probes where possible).
 * Designed for authorized security research and education only.
 *
 * Hardware: M5StickC Plus or Plus2
 * Libraries: M5Unified, M5GFX, NimBLE-Arduino (recommended)
 *
 * Controls:
 *   Button A (front M5): Select / Enter
 *   Button B (side): Next item / Scroll
 *   Power button short: Back / Home (Plus2 behavior varies)
 *
 * Author: barrydinya + Grok collaboration
 * License: MIT (for educational/authorized use)
 */

#include <M5Unified.h>
#include <WiFi.h>
#include <NimBLEDevice.h>   // Prefer NimBLE over Bluedroid for efficiency
#include <vector>
#include <string>

// ==================== CONFIG ====================
#define MAX_WIFI_RESULTS  20
#define MAX_BLE_RESULTS   30
#define SCAN_TIMEOUT_MS   8000
#define MENU_ITEMS        6

// Colors
#define COL_BG        BLACK
#define COL_TITLE     TFT_CYAN
#define COL_TEXT      TFT_WHITE
#define COL_HIGHLIGHT TFT_YELLOW
#define COL_STATUS    TFT_GREEN
#define COL_WARN      TFT_ORANGE
#define COL_ERR       TFT_RED

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
bool scanning = false;
unsigned long lastButtonTime = 0;
const unsigned long DEBOUNCE_MS = 180;

// Menu labels
const char* menuItems[MENU_ITEMS] = {
  "WiFi Recon",
  "BLE Recon",
  "Hybrid Scan",
  "View Logs",
  "Settings",
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

// ==================== FORWARD DECLS ====================
void drawHome();
void drawMenu();
void drawWifiResults();
void drawBleResults();
void drawAbout();
void drawStatusBar();
void handleButtons();
void startWifiScan();
void startBleScan();
void updateBattery();
String authModeToStr(wifi_auth_mode_t m);

// ==================== SETUP ====================
void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);
  M5.Display.setRotation(1);          // Landscape for better menu
  M5.Display.setBrightness(80);
  M5.Display.fillScreen(COL_BG);
  M5.Display.setTextDatum(top_left);

  // Init WiFi in station mode for scanning
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // Init NimBLE
  NimBLEDevice::init("WiFighter");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9); // Max TX for better range

  updateBattery();
  drawHome();
  Serial.println("[WiFighter] Boot complete - Authorized use only");
}

// ==================== LOOP ====================
void loop() {
  M5.update();
  handleButtons();

  // Periodic battery + light UI refresh on home
  static unsigned long lastRefresh = 0;
  if (millis() - lastRefresh > 5000) {
    updateBattery();
    if (currentState == STATE_HOME) drawHome();
    lastRefresh = millis();
  }

  delay(10);
}

// ==================== BUTTON HANDLING ====================
void handleButtons() {
  if (millis() - lastButtonTime < DEBOUNCE_MS) return;

  // Button A (M5 / front) - Select / Enter
  if (M5.BtnA.wasPressed()) {
    lastButtonTime = millis();
    switch (currentState) {
      case STATE_HOME:
        currentState = STATE_MENU;
        menuIndex = 0;
        drawMenu();
        break;
      case STATE_MENU:
        switch (menuIndex) {
          case 0: currentState = STATE_WIFI_SCAN; startWifiScan(); break;
          case 1: currentState = STATE_BLE_SCAN;  startBleScan();  break;
          case 2: currentState = STATE_HYBRID;    startWifiScan(); // then BLE
                                                  break;
          case 3: currentState = STATE_LOGS;      drawWifiResults(); // simple for now
                                                  break;
          case 4: currentState = STATE_SETTINGS;  statusMsg = "Settings TBD";
                                                  drawMenu(); break;
          case 5: currentState = STATE_ABOUT;     drawAbout(); break;
        }
        break;
      case STATE_WIFI_SCAN:
      case STATE_BLE_SCAN:
      case STATE_HYBRID:
      case STATE_LOGS:
      case STATE_ABOUT:
        // Select does nothing extra; B or power backs out
        break;
      default: break;
    }
  }

  // Button B (side) - Next / Scroll
  if (M5.BtnB.wasPressed()) {
    lastButtonTime = millis();
    if (currentState == STATE_MENU) {
      menuIndex = (menuIndex + 1) % MENU_ITEMS;
      drawMenu();
    } else if (currentState == STATE_WIFI_SCAN || currentState == STATE_LOGS) {
      scrollOffset = (scrollOffset + 1) % max(1, (int)wifiResults.size());
      drawWifiResults();
    } else if (currentState == STATE_BLE_SCAN) {
      scrollOffset = (scrollOffset + 1) % max(1, (int)bleResults.size());
      drawBleResults();
    }
  }

  // Power button short press often acts as "Home" on Plus2 / some firmwares
  // M5.BtnPWR exists on some configs; fallback to long BtnA or just use state
  if (M5.BtnPWR.wasClicked()) {   // if supported
    lastButtonTime = millis();
    currentState = STATE_HOME;
    drawHome();
  }
}

// ==================== DRAWING ====================
void drawStatusBar() {
  M5.Display.fillRect(0, 0, M5.Display.width(), 14, TFT_NAVY);
  M5.Display.setTextColor(COL_TEXT);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(2, 3);
  M5.Display.printf("WiFighter  Bat:%d%%", batteryPct);
  M5.Display.setCursor(M5.Display.width() - 40, 3);
  M5.Display.print(statusMsg.substring(0, 6));
}

void drawHome() {
  M5.Display.fillScreen(COL_BG);
  drawStatusBar();

  // Title
  M5.Display.setTextColor(COL_TITLE);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(18, 30);
  M5.Display.print("WiFighter");

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(COL_TEXT);
  M5.Display.setCursor(10, 55);
  M5.Display.print("BLE + WiFi Device Recon");

  M5.Display.setCursor(10, 75);
  M5.Display.setTextColor(COL_STATUS);
  M5.Display.print("Status: ");
  M5.Display.print(statusMsg);

  M5.Display.setTextColor(COL_TEXT);
  M5.Display.setCursor(10, 95);
  M5.Display.printf("WiFi entries: %d", wifiResults.size());
  M5.Display.setCursor(10, 108);
  M5.Display.printf("BLE entries : %d", bleResults.size());

  // Footer prompt
  M5.Display.setTextColor(COL_HIGHLIGHT);
  M5.Display.setCursor(10, 125);
  M5.Display.print("A: Menu   B: Scroll");
}

void drawMenu() {
  M5.Display.fillScreen(COL_BG);
  drawStatusBar();

  M5.Display.setTextColor(COL_TITLE);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(8, 18);
  M5.Display.print("== MAIN MENU ==");

  for (int i = 0; i < MENU_ITEMS; i++) {
    int y = 34 + i * 14;
    if (i == menuIndex) {
      M5.Display.fillRect(4, y - 2, M5.Display.width() - 8, 13, TFT_DARKGREY);
      M5.Display.setTextColor(COL_HIGHLIGHT);
      M5.Display.setCursor(10, y);
      M5.Display.print("> ");
      M5.Display.print(menuItems[i]);
    } else {
      M5.Display.setTextColor(COL_TEXT);
      M5.Display.setCursor(18, y);
      M5.Display.print(menuItems[i]);
    }
  }

  M5.Display.setTextColor(COL_WARN);
  M5.Display.setCursor(8, 125);
  M5.Display.print("A:Select  B:Next");
}

void drawWifiResults() {
  M5.Display.fillScreen(COL_BG);
  drawStatusBar();

  M5.Display.setTextColor(COL_TITLE);
  M5.Display.setCursor(4, 16);
  M5.Display.printf("WiFi (%d)  #%d", wifiResults.size(), scrollOffset + 1);

  if (wifiResults.empty()) {
    M5.Display.setTextColor(COL_WARN);
    M5.Display.setCursor(10, 50);
    M5.Display.print("No results yet.");
    M5.Display.setCursor(10, 65);
    M5.Display.print("Run a scan first.");
  } else {
    // Show one detailed entry + list peek
    const WifiEntry& e = wifiResults[scrollOffset % wifiResults.size()];
    M5.Display.setTextColor(COL_TEXT);
    M5.Display.setCursor(4, 32);
    M5.Display.printf("SSID: %.16s", e.ssid.c_str());
    M5.Display.setCursor(4, 45);
    M5.Display.printf("BSSID: %s", e.bssid.c_str());
    M5.Display.setCursor(4, 58);
    M5.Display.printf("RSSI: %d dBm  Ch:%d", e.rssi, e.channel);
    M5.Display.setCursor(4, 71);
    M5.Display.printf("Enc: %s", authModeToStr(e.enc).c_str());

    // Mini list of next few
    M5.Display.setTextColor(TFT_LIGHTGREY);
    int shown = 0;
    for (size_t i = 1; i < wifiResults.size() && shown < 3; i++) {
      size_t idx = (scrollOffset + i) % wifiResults.size();
      M5.Display.setCursor(4, 90 + shown * 11);
      M5.Display.printf("%s %ddB", wifiResults[idx].ssid.substring(0, 12).c_str(),
                        wifiResults[idx].rssi);
      shown++;
    }
  }

  M5.Display.setTextColor(COL_HIGHLIGHT);
  M5.Display.setCursor(4, 125);
  M5.Display.print("B:Next  PWR:Home");
}

void drawBleResults() {
  M5.Display.fillScreen(COL_BG);
  drawStatusBar();

  M5.Display.setTextColor(COL_TITLE);
  M5.Display.setCursor(4, 16);
  M5.Display.printf("BLE (%d)  #%d", bleResults.size(), scrollOffset + 1);

  if (bleResults.empty()) {
    M5.Display.setTextColor(COL_WARN);
    M5.Display.setCursor(10, 50);
    M5.Display.print("No BLE devices.");
  } else {
    const BleEntry& e = bleResults[scrollOffset % bleResults.size()];
    M5.Display.setTextColor(COL_TEXT);
    M5.Display.setCursor(4, 32);
    M5.Display.printf("Name: %.14s", e.name.c_str());
    M5.Display.setCursor(4, 45);
    M5.Display.printf("MAC: %s", e.address.c_str());
    M5.Display.setCursor(4, 58);
    M5.Display.printf("RSSI: %d dBm", e.rssi);
    if (e.manufacturer.length() > 0) {
      M5.Display.setCursor(4, 71);
      M5.Display.printf("Mfr: %.16s", e.manufacturer.c_str());
    }
  }

  M5.Display.setTextColor(COL_HIGHLIGHT);
  M5.Display.setCursor(4, 125);
  M5.Display.print("B:Next  PWR:Home");
}

void drawAbout() {
  M5.Display.fillScreen(COL_BG);
  drawStatusBar();
  M5.Display.setTextColor(COL_TITLE);
  M5.Display.setCursor(8, 20);
  M5.Display.print("WiFighter v0.1");
  M5.Display.setTextColor(COL_TEXT);
  M5.Display.setCursor(8, 40);
  M5.Display.print("Authorized research");
  M5.Display.setCursor(8, 52);
  M5.Display.print("only. No illegal use.");
  M5.Display.setCursor(8, 70);
  M5.Display.print("M5StickC Plus/Plus2");
  M5.Display.setCursor(8, 82);
  M5.Display.print("Arduino + NimBLE");
  M5.Display.setCursor(8, 100);
  M5.Display.print("github.com/barrydinya");
  M5.Display.setTextColor(COL_HIGHLIGHT);
  M5.Display.setCursor(8, 125);
  M5.Display.print("PWR / A long: Home");
}

// ==================== SCANS ====================
void startWifiScan() {
  statusMsg = "WiFi...";
  scanning = true;
  drawHome(); // quick feedback

  wifiResults.clear();
  scrollOffset = 0;

  int n = WiFi.scanNetworks(false, true); // async=false, show_hidden=true
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
  Serial.printf("[WiFi] Found %d networks\n", wifiResults.size());
}

void startBleScan() {
  statusMsg = "BLE...";
  scanning = true;
  drawHome();

  bleResults.clear();
  scrollOffset = 0;

  NimBLEScan* pScan = NimBLEDevice::getScan();
  pScan->setActiveScan(true);
  pScan->setInterval(100);
  pScan->setWindow(99);
  pScan->setMaxResults(0); // unlimited, we filter

  NimBLEScanResults results = pScan->start(SCAN_TIMEOUT_MS / 1000, false);

  int count = results.getCount();
  for (int i = 0; i < count && bleResults.size() < MAX_BLE_RESULTS; i++) {
    const NimBLEAdvertisedDevice* dev = results.getDevice(i);
    BleEntry e;
    e.address = dev->getAddress().toString().c_str();
    e.rssi    = dev->getRSSI();
    e.name    = dev->haveName() ? dev->getName().c_str() : "(unknown)";
    e.manufacturer = "";
    if (dev->haveManufacturerData()) {
      std::string md = dev->getManufacturerData();
      // simple hex preview
      char buf[24];
      snprintf(buf, sizeof(buf), "%02X%02X...", (uint8_t)md[0], (uint8_t)md[1]);
      e.manufacturer = buf;
    }
    bleResults.push_back(e);
  }
  pScan->clearResults();

  statusMsg = "BLE OK";
  scanning = false;
  currentState = STATE_BLE_SCAN;
  drawBleResults();
  Serial.printf("[BLE] Found %d devices\n", bleResults.size());
}

// ==================== HELPERS ====================
void updateBattery() {
  // M5Unified provides battery level estimation
  batteryPct = M5.Power.getBatteryLevel();
  if (batteryPct < 0) batteryPct = 0;
  if (batteryPct > 100) batteryPct = 100;
}

String authModeToStr(wifi_auth_mode_t m) {
  switch (m) {
    case WIFI_AUTH_OPEN:         return "OPEN";
    case WIFI_AUTH_WEP:          return "WEP";
    case WIFI_AUTH_WPA_PSK:      return "WPA";
    case WIFI_AUTH_WPA2_PSK:     return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENT";
    case WIFI_AUTH_WPA3_PSK:     return "WPA3";
    default:                     return "UNK";
  }
}
