/*
 * WiFighter v0.7 - Portable WiFi/BLE Device Data Recon Tool
 * For M5StickC Plus / M5StickC Plus2
 *
 * Collects nearby WiFi AP info, BLE advertisements, residual device
 * identifiers (MACs, names, manufacturer data) and continuous probe-request
 * sniffing (clients still looking for networks they left). Tracks devices
 * that were previously seen but have left the current scan ("ghosts").
 * Designed for authorized security research and education only.
 *
 * Hardware: M5StickC Plus or Plus2 (Plus2 recommended for RF)
 * Libraries: M5Unified, M5GFX, NimBLE-Arduino
 *
 * Controls:
 *   Button A (front M5): Select / Enter / Back (context)
 *   Button B (side):     Next item / Scroll / Clear
 *   Power button short:  Home (Plus2 / supported boards)
 *
 * Author: barrydinya + Grok collaboration
 * License: MIT (educational / authorized use only)
 */

#include <M5Unified.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <vector>
#include <string>
#include <algorithm>
#include <set>

// ==================== CONFIG ====================
#define MAX_WIFI_RESULTS   30
#define MAX_BLE_RESULTS    40
#define MAX_GHOSTS         25
#define MAX_PROBES         40
#define DEFAULT_SCAN_SEC   6
#define MENU_ITEMS         10
#define DEBOUNCE_MS        160
#define IDLE_SLEEP_MS      180000UL   // 3 min default if autoSleep enabled
#define CHANNEL_HOP_MS     260
#define PROBE_RING_SIZE    16         // ISR-safe ring buffer size

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
#define COL_GHOST      TFT_MAGENTA
#define COL_ALERT      TFT_RED

// ==================== STATE ====================
enum AppState {
  STATE_HOME,
  STATE_MENU,
  STATE_WIFI_SCAN,
  STATE_BLE_SCAN,
  STATE_HYBRID,
  STATE_PROBE_SNIFF,
  STATE_GHOSTS,
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
int proxThreshold = -55;          // RSSI alert threshold (dBm). 0 = disabled
bool proxAlertEnabled = false;

// Menu labels
const char* menuItems[MENU_ITEMS] = {
  "WiFi Recon",
  "BLE Recon",
  "Hybrid Scan",
  "Probe Sniff",
  "View Results",
  "Ghosts / Left",
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
  unsigned long lastSeen;
  bool present;
};

struct BleEntry {
  String name;
  String address;
  int rssi;
  String manufacturer;
  unsigned long lastSeen;
  bool present;
};

struct ProbeEntry {
  String mac;
  String ssid;
  int8_t rssi;
  unsigned long lastSeen;
};

// ISR-safe probe ring (fixed size, no String/heap)
struct ProbeRaw {
  uint8_t mac[6];
  char ssid[33];
  int8_t rssi;
  bool valid;
};

volatile ProbeRaw probeRing[PROBE_RING_SIZE];
volatile uint8_t probeHead = 0;
volatile uint8_t probeTail = 0;

std::vector<WifiEntry> wifiResults;
std::vector<BleEntry> bleResults;
std::vector<WifiEntry> wifiGhosts;
std::vector<BleEntry> bleGhosts;
std::vector<ProbeEntry> probeResults;
String statusMsg = "Ready";
int batteryPct = 100;
Preferences prefs;
bool probeSniffing = false;
uint8_t currentChannel = 1;
unsigned long lastChannelHop = 0;
unsigned long lastProbeUi = 0;
unsigned long lastAlertFlash = 0;
bool alertActive = false;

// ==================== FORWARD DECLS ====================
void drawHome();
void drawMenu();
void drawWifiResults();
void drawBleResults();
void drawHybridResults();
void drawGhosts();
void drawProbeSniff();
void drawSettings();
void drawAbout();
void drawStatusBar();
void drawRssiBar(int x, int y, int rssi, int maxW = 60);
void handleButtons();
void startWifiScan();
void startBleScan();
void startHybridScan();
void startProbeSniff();
void stopProbeSniff();
void processProbeRing();
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
String timeAgo(unsigned long t);
void checkProximityAlert();

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

  // Clear probe ring
  for (int i = 0; i < PROBE_RING_SIZE; i++) {
    probeRing[i].valid = false;
  }

  lastActivity = millis();
  updateBattery();
  drawHome();
  Serial.println("[WiFighter v0.7] Boot complete - Authorized use only");
}

// ==================== LOOP ====================
void loop() {
  M5.update();
  handleButtons();

  // Drain ISR-safe probe ring into main results
  if (probeSniffing) {
    processProbeRing();
  }

  // Periodic refresh on home + proximity check
  if (millis() - lastRefresh > 3200) {
    updateBattery();
    if (currentState == STATE_HOME) {
      checkProximityAlert();
      drawHome();
    }
    lastRefresh = millis();
  }

  // Probe sniff: channel hop + UI refresh
  if (probeSniffing) {
    unsigned long now = millis();
    if (now - lastChannelHop > CHANNEL_HOP_MS) {
      currentChannel++;
      if (currentChannel > 13) currentChannel = 1;
      esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
      lastChannelHop = now;
    }
    if (now - lastProbeUi > 1000) {
      drawProbeSniff();
      lastProbeUi = now;
      lastActivity = now;
    }
  }

  // Flash alert on home if proximity triggered
  if (alertActive && currentState == STATE_HOME && (millis() - lastAlertFlash > 400)) {
    lastAlertFlash = millis();
    // brief visual handled inside drawHome
  }

  // Optional auto deep-sleep after idle
  if (autoSleep && !scanning && !probeSniffing && (millis() - lastActivity > IDLE_SLEEP_MS)) {
    statusMsg = "Sleep";
    drawHome();
    delay(350);
    M5.Power.deepSleep(0);
  }

  delay(5);
}

// ==================== NAV HELPERS ====================
void goHome() {
  stopProbeSniff();
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
          case 3: startProbeSniff(); break;
          case 4: // View Results
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
            } else if (!probeResults.empty()) {
              currentState = STATE_PROBE_SNIFF;
              scrollOffset = 0;
              drawProbeSniff();
            } else {
              statusMsg = "No data";
              drawMenu();
            }
            break;
          case 5:
            currentState = STATE_GHOSTS;
            scrollOffset = 0;
            drawGhosts();
            break;
          case 6:
            exportSerial();
            break;
          case 7:
            currentState = STATE_SETTINGS;
            settingsIndex = 0;
            drawSettings();
            break;
          case 8:
            clearAllData();
            statusMsg = "Cleared";
            drawMenu();
            break;
          case 9:
            currentState = STATE_ABOUT;
            drawAbout();
            break;
        }
        break;

      case STATE_WIFI_SCAN:
      case STATE_BLE_SCAN:
      case STATE_HYBRID:
      case STATE_PROBE_SNIFF:
        stopProbeSniff();
        // fallthrough
      case STATE_GHOSTS:
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
        } else if (settingsIndex == 3) {    // Proximity alert toggle + threshold
          proxAlertEnabled = !proxAlertEnabled;
          if (proxAlertEnabled && proxThreshold == 0) proxThreshold = -55;
        } else if (settingsIndex == 4) {    // Adjust threshold
          if (proxAlertEnabled) {
            proxThreshold += 5;
            if (proxThreshold > -30) proxThreshold = -80;
          }
        } else if (settingsIndex == 5) {    // Save & Exit
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
      settingsIndex = (settingsIndex + 1) % 6;
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
    else if (currentState == STATE_GHOSTS) {
      int total = (int)wifiGhosts.size() + (int)bleGhosts.size();
      if (total > 0) {
        scrollOffset = (scrollOffset + 1) % total;
        drawGhosts();
      }
    }
    else if (currentState == STATE_PROBE_SNIFF) {
      probeResults.clear();
      // also clear ring
      for (int i = 0; i < PROBE_RING_SIZE; i++) probeRing[i].valid = false;
      probeHead = probeTail = 0;
      drawProbeSniff();
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

  // simple battery bar
  int barW = map(batteryPct, 0, 100, 0, 18);
  M5.Display.drawRect(M5.Display.width() - 74, 3, 20, 8, COL_DIM);
  if (barW > 0) {
    uint16_t bcol = batteryPct > 30 ? COL_STATUS : COL_WARN;
    M5.Display.fillRect(M5.Display.width() - 73, 4, barW, 6, bcol);
  }

  M5.Display.setCursor(M5.Display.width() - 50, 2);
  M5.Display.print(statusMsg.substring(0, 7));
}

void drawRssiBar(int x, int y, int rssi, int maxW) {
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

  // Title
  M5.Display.setTextColor(COL_TITLE);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(8, 16);
  M5.Display.print("WiFighter");

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(COL_DIM);
  M5.Display.setCursor(10, 36);
  M5.Display.print("BLE + WiFi Recon  v0.7");

  // Status
  M5.Display.setTextColor(COL_STATUS);
  M5.Display.setCursor(6, 52);
  M5.Display.print("Status: ");
  M5.Display.setTextColor(alertActive ? COL_ALERT : COL_TEXT);
  M5.Display.print(statusMsg);

  // Live counts - two columns style
  int ghosts = (int)wifiGhosts.size() + (int)bleGhosts.size();
  M5.Display.setTextColor(COL_TEXT);
  M5.Display.setCursor(6, 68);
  M5.Display.printf("WiFi:%2d  BLE:%2d", (int)wifiResults.size(), (int)bleResults.size());

  M5.Display.setTextColor(COL_GHOST);
  M5.Display.setCursor(6, 82);
  M5.Display.printf("Ghosts:%d  Probes:%d", ghosts, (int)probeResults.size());

  // Last scan + proximity hint
  M5.Display.setTextColor(COL_DIM);
  M5.Display.setCursor(6, 98);
  M5.Display.print(timeSinceLastScan());

  if (proxAlertEnabled) {
    M5.Display.setTextColor(alertActive ? COL_ALERT : COL_DIM);
    M5.Display.setCursor(6, 110);
    M5.Display.printf("Prox:%s  thr %d", alertActive ? "NEAR!" : "armed", proxThreshold);
  }

  // Hint
  M5.Display.setTextColor(COL_HIGHLIGHT);
  M5.Display.setCursor(6, 122);
  M5.Display.print("A:Menu   PWR:Home");
}

void drawMenu() {
  M5.Display.fillScreen(COL_BG);
  drawStatusBar();

  M5.Display.setTextColor(COL_TITLE);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(6, 15);
  M5.Display.print("== MAIN MENU ==");

  // Scroll window of 6 items
  int start = 0;
  if (menuIndex > 5) start = menuIndex - 5;

  for (int i = 0; i < 6 && (start + i) < MENU_ITEMS; i++) {
    int idx = start + i;
    int y = 28 + i * 13;
    if (idx == menuIndex) {
      M5.Display.fillRect(2, y - 1, M5.Display.width() - 4, 13, TFT_DARKGREY);
      M5.Display.setTextColor(COL_HIGHLIGHT);
      M5.Display.setCursor(4, y);
      M5.Display.print("> ");
      M5.Display.print(menuItems[idx]);
    } else {
      M5.Display.setTextColor(COL_TEXT);
      M5.Display.setCursor(14, y);
      M5.Display.print(menuItems[idx]);
    }
  }

  // Scroll indicator
  if (MENU_ITEMS > 6) {
    M5.Display.setTextColor(COL_DIM);
    M5.Display.setCursor(M5.Display.width() - 28, 15);
    M5.Display.printf("%d/%d", menuIndex + 1, MENU_ITEMS);
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
      M5.Display.setCursor(4, 30);
      M5.Display.print("[WiFi]");
      M5.Display.setTextColor(COL_TEXT);
      M5.Display.setCursor(4, 44);
      M5.Display.printf("%.16s", e.ssid.c_str());
      M5.Display.setCursor(4, 58);
      M5.Display.printf("%s  %ddB", e.bssid.c_str(), e.rssi);
      drawRssiBar(4, 72, e.rssi, 70);
      M5.Display.setCursor(4, 84);
      M5.Display.printf("Ch:%d  %s", e.channel, authModeToStr(e.enc).c_str());
    } else {
      int bIdx = idx - (int)wifiResults.size();
      const BleEntry& e = bleResults[bIdx];
      M5.Display.setTextColor(COL_STATUS);
      M5.Display.setCursor(4, 30);
      M5.Display.print("[BLE]");
      M5.Display.setTextColor(COL_TEXT);
      M5.Display.setCursor(4, 44);
      M5.Display.printf("%.16s", e.name.c_str());
      M5.Display.setCursor(4, 58);
      M5.Display.printf("%s", e.address.c_str());
      drawRssiBar(4, 72, e.rssi, 70);
      M5.Display.setCursor(4, 84);
      M5.Display.printf("%d dBm  %s", e.rssi, e.manufacturer.c_str());
    }
    M5.Display.setTextColor(COL_DIM);
    M5.Display.setCursor(4, 100);
    M5.Display.printf("Item %d of %d", idx + 1, total);
  }

  M5.Display.setTextColor(COL_HIGHLIGHT);
  M5.Display.setCursor(4, 122);
  M5.Display.print("B:Next  A:Back");
}

void drawGhosts() {
  M5.Display.fillScreen(COL_BG);
  drawStatusBar();

  int total = (int)wifiGhosts.size() + (int)bleGhosts.size();
  M5.Display.setTextColor(COL_GHOST);
  M5.Display.setCursor(4, 15);
  M5.Display.printf("Ghosts %d  (left)", total);

  if (total == 0) {
    M5.Display.setTextColor(COL_WARN);
    M5.Display.setCursor(8, 50);
    M5.Display.print("No ghosts yet.");
    M5.Display.setCursor(8, 64);
    M5.Display.print("Scan, then re-scan");
    M5.Display.setCursor(8, 78);
    M5.Display.print("to detect leavers.");
  } else {
    int idx = scrollOffset % total;
    if (idx < (int)wifiGhosts.size()) {
      const WifiEntry& e = wifiGhosts[idx];
      M5.Display.setTextColor(COL_STATUS);
      M5.Display.setCursor(4, 30);
      M5.Display.print("[WiFi LEFT]");
      M5.Display.setTextColor(COL_TEXT);
      M5.Display.setCursor(4, 44);
      M5.Display.printf("%.16s", e.ssid.c_str());
      M5.Display.setCursor(4, 58);
      M5.Display.printf("%s", e.bssid.c_str());
      M5.Display.setCursor(4, 72);
      M5.Display.printf("Last: %s", timeAgo(e.lastSeen).c_str());
      M5.Display.setCursor(4, 86);
      M5.Display.printf("Was %d dBm  Ch:%d", e.rssi, e.channel);
    } else {
      int bIdx = idx - (int)wifiGhosts.size();
      const BleEntry& e = bleGhosts[bIdx];
      M5.Display.setTextColor(COL_STATUS);
      M5.Display.setCursor(4, 30);
      M5.Display.print("[BLE LEFT]");
      M5.Display.setTextColor(COL_TEXT);
      M5.Display.setCursor(4, 44);
      M5.Display.printf("%.16s", e.name.c_str());
      M5.Display.setCursor(4, 58);
      M5.Display.printf("%s", e.address.c_str());
      M5.Display.setCursor(4, 72);
      M5.Display.printf("Last: %s", timeAgo(e.lastSeen).c_str());
      M5.Display.setCursor(4, 86);
      M5.Display.printf("Was %d dBm", e.rssi);
    }
    M5.Display.setTextColor(COL_DIM);
    M5.Display.setCursor(4, 102);
    M5.Display.printf("Item %d of %d", idx + 1, total);
  }

  M5.Display.setTextColor(COL_HIGHLIGHT);
  M5.Display.setCursor(4, 122);
  M5.Display.print("B:Next  A:Back");
}

void drawProbeSniff() {
  M5.Display.fillScreen(COL_BG);
  drawStatusBar();

  M5.Display.setTextColor(COL_TITLE);
  M5.Display.setCursor(4, 15);
  M5.Display.printf("Probe Sniff  Ch:%d  %s", currentChannel, probeSniffing ? "RUN" : "STOP");

  M5.Display.setTextColor(COL_STATUS);
  M5.Display.setCursor(4, 28);
  M5.Display.printf("Unique probes: %d", (int)probeResults.size());

  if (probeResults.empty()) {
    M5.Display.setTextColor(COL_WARN);
    M5.Display.setCursor(8, 55);
    M5.Display.print("Listening for probes...");
    M5.Display.setCursor(8, 70);
    M5.Display.print("(clients seeking nets)");
  } else {
    int shown = 0;
    int y = 42;
    for (int i = (int)probeResults.size() - 1; i >= 0 && shown < 5; i--) {
      const ProbeEntry& e = probeResults[i];
      M5.Display.setTextColor(COL_TEXT);
      M5.Display.setCursor(4, y);
      String macShort = e.mac.substring(9);
      String ss = e.ssid.length() > 0 ? e.ssid.substring(0, 9) : "<bcast>";
      M5.Display.printf("%s %s", macShort.c_str(), ss.c_str());
      M5.Display.setCursor(M5.Display.width() - 28, y);
      M5.Display.printf("%d", e.rssi);
      y += 13;
      shown++;
    }
  }

  M5.Display.setTextColor(COL_HIGHLIGHT);
  M5.Display.setCursor(4, 122);
  M5.Display.print("A:Stop/Back  B:Clear");
}

void drawSettings() {
  M5.Display.fillScreen(COL_BG);
  drawStatusBar();

  M5.Display.setTextColor(COL_TITLE);
  M5.Display.setCursor(6, 15);
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

  drawItem(0, 28, "Scan time:", String(scanSeconds) + "s");
  drawItem(1, 41, "Brightness:", String(brightness));
  drawItem(2, 54, "Auto-sleep:", autoSleep ? "ON" : "OFF");
  drawItem(3, 67, "Prox alert:", proxAlertEnabled ? "ON" : "OFF");
  drawItem(4, 80, "Prox thr:", String(proxThreshold) + "dBm");
  drawItem(5, 93, "Save & Exit", "");

  M5.Display.setTextColor(COL_DIM);
  M5.Display.setCursor(6, 110);
  M5.Display.print("A:Change  B:Next");

  M5.Display.setTextColor(COL_WARN);
  M5.Display.setCursor(6, 122);
  M5.Display.print("Persists in NVS");
}

void drawAbout() {
  M5.Display.fillScreen(COL_BG);
  drawStatusBar();

  M5.Display.setTextColor(COL_TITLE);
  M5.Display.setCursor(6, 16);
  M5.Display.print("WiFighter v0.7");

  M5.Display.setTextColor(COL_TEXT);
  M5.Display.setCursor(6, 34);
  M5.Display.print("Authorized research");
  M5.Display.setCursor(6, 46);
  M5.Display.print("only. No illegal use.");

  M5.Display.setCursor(6, 64);
  M5.Display.print("M5StickC Plus/Plus2");
  M5.Display.setCursor(6, 76);
  M5.Display.print("Arduino + NimBLE");

  M5.Display.setCursor(6, 94);
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

void checkProximityAlert() {
  alertActive = false;
  if (!proxAlertEnabled) return;

  for (const auto& e : wifiResults) {
    if (e.rssi >= proxThreshold) {
      alertActive = true;
      statusMsg = "NEAR!";
      return;
    }
  }
  for (const auto& e : bleResults) {
    if (e.rssi >= proxThreshold) {
      alertActive = true;
      statusMsg = "NEAR!";
      return;
    }
  }
}

// ==================== PROBE REQUEST SNIFFER (ISR-safe) ====================
// Callback only writes to fixed ring buffer. No String, no heap, no set.
void IRAM_ATTR wifi_sniffer_cb(void* buf, wifi_pkt_rx_ctrl_t* ctrl) {
  if (!probeSniffing) return;
  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t*)buf;
  uint8_t *payload = pkt->payload;
  int len = ctrl->sig_len;
  if (len < 24) return;
  // Probe Request = type/subtype 0x40
  if ((payload[0] & 0xFC) != 0x40) return;

  uint8_t next = (probeHead + 1) % PROBE_RING_SIZE;
  if (next == probeTail) return; // ring full, drop

  ProbeRaw &slot = const_cast<ProbeRaw&>(probeRing[probeHead]);
  slot.mac[0] = payload[10];
  slot.mac[1] = payload[11];
  slot.mac[2] = payload[12];
  slot.mac[3] = payload[13];
  slot.mac[4] = payload[14];
  slot.mac[5] = payload[15];
  slot.rssi = ctrl->rssi;
  slot.ssid[0] = 0;

  int pos = 24;
  while (pos + 2 < len) {
    uint8_t tag = payload[pos];
    uint8_t tagLen = payload[pos + 1];
    if (tag == 0x00 && tagLen > 0 && tagLen < 33) {
      for (int i = 0; i < tagLen; i++) {
        slot.ssid[i] = (char)payload[pos + 2 + i];
      }
      slot.ssid[tagLen] = 0;
      break;
    }
    pos += 2 + tagLen;
  }
  slot.valid = true;
  probeHead = next;
}

void processProbeRing() {
  while (probeTail != probeHead) {
    ProbeRaw slot = probeRing[probeTail];
    probeRing[probeTail].valid = false;
    probeTail = (probeTail + 1) % PROBE_RING_SIZE;

    if (!slot.valid) continue;

    char macStr[18];
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
            slot.mac[0], slot.mac[1], slot.mac[2],
            slot.mac[3], slot.mac[4], slot.mac[5]);
    String mac = String(macStr);
    String ssid = String(slot.ssid);

    bool found = false;
    for (auto& e : probeResults) {
      if (e.mac == mac) {
        e.rssi = slot.rssi;
        e.lastSeen = millis();
        if (ssid.length() > 0) e.ssid = ssid;
        found = true;
        break;
      }
    }
    if (!found) {
      if (probeResults.size() >= MAX_PROBES) {
        probeResults.erase(probeResults.begin());
      }
      ProbeEntry e;
      e.mac = mac;
      e.ssid = ssid;
      e.rssi = slot.rssi;
      e.lastSeen = millis();
      probeResults.push_back(e);
    }
  }
}

void startProbeSniff() {
  stopProbeSniff();
  statusMsg = "Probe...";
  scanning = true;
  lastActivity = millis();
  drawHome();
  delay(25);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(40);

  // reset ring
  for (int i = 0; i < PROBE_RING_SIZE; i++) probeRing[i].valid = false;
  probeHead = probeTail = 0;

  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_cb);
  currentChannel = 1;
  esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
  probeSniffing = true;
  lastChannelHop = millis();
  lastProbeUi = millis();
  currentState = STATE_PROBE_SNIFF;
  statusMsg = "Sniffing";
  scanning = false;
  drawProbeSniff();
  Serial.println("[Probe] Continuous probe-request sniff started (ISR-safe ring)");
}

void stopProbeSniff() {
  if (probeSniffing) {
    esp_wifi_set_promiscuous(false);
    probeSniffing = false;
    processProbeRing(); // drain remaining
    Serial.println("[Probe] Sniff stopped");
  }
}

void startWifiScan() {
  statusMsg = "WiFi...";
  scanning = true;
  lastActivity = millis();
  drawHome();
  delay(20);

  for (const auto& e : wifiResults) {
    wifiGhosts.push_back(e);
  }
  while (wifiGhosts.size() > MAX_GHOSTS) wifiGhosts.erase(wifiGhosts.begin());

  wifiResults.clear();
  scrollOffset = 0;

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
    e.lastSeen = millis();
    e.present  = true;
    wifiResults.push_back(e);
  }
  WiFi.scanDelete();
  sortWifiByRssi();

  wifiGhosts.erase(
    std::remove_if(wifiGhosts.begin(), wifiGhosts.end(),
      [&](const WifiEntry& g) {
        for (const auto& c : wifiResults) {
          if (c.bssid == g.bssid) return true;
        }
        return false;
      }),
    wifiGhosts.end());

  lastScanTime = millis();
  statusMsg = "WiFi OK";
  scanning = false;
  currentState = STATE_WIFI_SCAN;
  drawWifiResults();
  Serial.printf("[WiFi] %d networks, %d ghosts\n", wifiResults.size(), wifiGhosts.size());
}

void startBleScan() {
  statusMsg = "BLE...";
  scanning = true;
  lastActivity = millis();
  drawHome();
  delay(20);

  for (const auto& e : bleResults) {
    bleGhosts.push_back(e);
  }
  while (bleGhosts.size() > MAX_GHOSTS) bleGhosts.erase(bleGhosts.begin());

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
    e.lastSeen = millis();
    e.present  = true;
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

  bleGhosts.erase(
    std::remove_if(bleGhosts.begin(), bleGhosts.end(),
      [&](const BleEntry& g) {
        for (const auto& c : bleResults) {
          if (c.address == g.address) return true;
        }
        return false;
      }),
    bleGhosts.end());

  lastScanTime = millis();
  statusMsg = "BLE OK";
  scanning = false;
  currentState = STATE_BLE_SCAN;
  drawBleResults();
  Serial.printf("[BLE] %d devices (deduped), %d ghosts\n", bleResults.size(), bleGhosts.size());
}

void startHybridScan() {
  statusMsg = "Hybrid...";
  scanning = true;
  lastActivity = millis();
  drawHome();
  delay(15);

  for (const auto& e : wifiResults) wifiGhosts.push_back(e);
  for (const auto& e : bleResults)  bleGhosts.push_back(e);
  while (wifiGhosts.size() > MAX_GHOSTS) wifiGhosts.erase(wifiGhosts.begin());
  while (bleGhosts.size()  > MAX_GHOSTS) bleGhosts.erase(bleGhosts.begin());

  // WiFi
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
    e.lastSeen = millis();
    e.present  = true;
    wifiResults.push_back(e);
  }
  WiFi.scanDelete();
  sortWifiByRssi();

  // BLE
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
    e.lastSeen = millis();
    e.present  = true;
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

  wifiGhosts.erase(
    std::remove_if(wifiGhosts.begin(), wifiGhosts.end(),
      [&](const WifiEntry& g) {
        for (const auto& c : wifiResults) if (c.bssid == g.bssid) return true;
        return false;
      }),
    wifiGhosts.end());
  bleGhosts.erase(
    std::remove_if(bleGhosts.begin(), bleGhosts.end(),
      [&](const BleEntry& g) {
        for (const auto& c : bleResults) if (c.address == g.address) return true;
        return false;
      }),
    bleGhosts.end());

  lastScanTime = millis();
  statusMsg = "Done";
  scanning = false;
  currentState = STATE_HYBRID;
  scrollOffset = 0;
  drawHybridResults();
  Serial.printf("[Hybrid] WiFi:%d  BLE:%d  Ghosts:%d\n",
                wifiResults.size(), bleResults.size(),
                wifiGhosts.size() + bleGhosts.size());
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
  Serial.printf("\nGhosts (left): WiFi %d  BLE %d\n", wifiGhosts.size(), bleGhosts.size());
  for (const auto& e : wifiGhosts) {
    Serial.printf("  [W] %s | %s | last %s\n", e.ssid.c_str(), e.bssid.c_str(), timeAgo(e.lastSeen).c_str());
  }
  for (const auto& e : bleGhosts) {
    Serial.printf("  [B] %s | %s | last %s\n", e.name.c_str(), e.address.c_str(), timeAgo(e.lastSeen).c_str());
  }
  Serial.printf("\nProbes: %d\n", probeResults.size());
  for (const auto& e : probeResults) {
    Serial.printf("  %s | %s | %d dBm\n", e.mac.c_str(), e.ssid.c_str(), e.rssi);
  }
  Serial.println("======================================");

  statusMsg = "Exported";
  delay(350);
  goMenu();
}

void clearAllData() {
  wifiResults.clear();
  bleResults.clear();
  wifiGhosts.clear();
  bleGhosts.clear();
  probeResults.clear();
  for (int i = 0; i < PROBE_RING_SIZE; i++) probeRing[i].valid = false;
  probeHead = probeTail = 0;
  scrollOffset = 0;
  lastScanTime = 0;
  alertActive = false;
  statusMsg = "Cleared";
  Serial.println("[Clear] All results + ghosts + probes wiped");
}

// ==================== PERSISTENCE ====================
void loadSettings() {
  scanSeconds     = prefs.getInt("scanSec", DEFAULT_SCAN_SEC);
  brightness      = prefs.getInt("bright", 80);
  autoSleep       = prefs.getBool("autoSleep", false);
  proxAlertEnabled = prefs.getBool("proxOn", false);
  proxThreshold   = prefs.getInt("proxThr", -55);
  if (scanSeconds < 4 || scanSeconds > 14) scanSeconds = DEFAULT_SCAN_SEC;
  if (brightness < 40 || brightness > 100) brightness = 80;
  if (proxThreshold < -90 || proxThreshold > -20) proxThreshold = -55;
}

void saveSettings() {
  prefs.putInt("scanSec", scanSeconds);
  prefs.putInt("bright", brightness);
  prefs.putBool("autoSleep", autoSleep);
  prefs.putBool("proxOn", proxAlertEnabled);
  prefs.putInt("proxThr", proxThreshold);
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

String timeAgo(unsigned long t) {
  if (t == 0) return "unknown";
  unsigned long sec = (millis() - t) / 1000;
  if (sec < 60) return String(sec) + "s ago";
  if (sec < 3600) return String(sec / 60) + "m ago";
  return String(sec / 3600) + "h ago";
}
