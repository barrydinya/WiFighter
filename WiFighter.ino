/*
 * WiFighter — M5StickC Plus / Plus2
 * BLE + WiFi leftover / persistence tracker
 *
 * Authorized security research / personal privacy monitoring only.
 * Unauthorized tracking or disruption is illegal.
 *
 * Controls
 *   BtnB       open Menu / next item / scroll
 *   BtnA       select / enter / back / toggle scan on Home
 *   Hold BtnB  jump Home from any screen
 */

#include <M5Unified.h>
#include <WiFi.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include "oui.h"

#define FW_VERSION "0.4.0"

static uint32_t SCAN_PERIOD_MS = 5000;
static uint32_t BLE_SCAN_MS    = 2800;
static uint32_t LEFT_AFTER_MS  = 45000;
static const float HIGH_PERSIST = 0.45f;
static const int   MAX_DEVICES  = 80;
static const int   VISIBLE_ROWS = 5;

enum RadioKind : uint8_t { KIND_BLE = 1, KIND_WIFI = 2, KIND_BOTH = 3 };
enum Screen : uint8_t {
  SCR_HOME, SCR_MENU, SCR_DEVICES, SCR_ALERTS, SCR_SETTINGS, SCR_ABOUT, SCR_DETAIL
};

struct Tracked {
  uint8_t  mac[6];
  char     name[18];
  int8_t   rssi;
  uint8_t  kind;
  uint16_t hits;
  uint32_t firstSeen;
  uint32_t lastSeen;
  float    persist;
  bool     left;
  bool     used;
};

static Tracked g_dev[MAX_DEVICES];
static int g_count = 0;

static Screen g_screen = SCR_HOME;
static int g_menuIdx = 0;
static int g_listOff = 0;
static int g_listSel = 0;
static int g_detailIdx = -1;
static int g_setIdx = 0;

static bool g_scanning = true;
static bool g_wifiOn = true;
static bool g_bleOn = true;
static bool g_scanBusy = false;
static uint32_t g_lastScan = 0;
static uint32_t g_bootMs = 0;
static uint32_t g_btnBDown = 0;
static bool g_holdConsumed = false;
static char g_status[28] = "idle";
static Preferences g_prefs;

static const char* MENU_ITEMS[] = {
  "Start/Stop Scan",
  "Device List",
  "Alerts / Left",
  "Settings",
  "Clear table",
  "About",
  "Back to Home"
};
static const int MENU_N = 7;

static uint16_t COL_BG      = 0x0841;
static uint16_t COL_PANEL   = 0x1082;
static uint16_t COL_ACCENT  = 0x07FF;
static uint16_t COL_OK      = 0x07E0;
static uint16_t COL_WARN    = 0xFD20;
static uint16_t COL_BAD     = 0xF800;
static uint16_t COL_MUTED   = 0x8410;
static uint16_t COL_TEXT    = 0xFFFF;
static uint16_t COL_DIM     = 0xC618;

static bool macEq(const uint8_t* a, const uint8_t* b) {
  return memcmp(a, b, 6) == 0;
}

static void macStr(const uint8_t* m, char* out, bool shortForm) {
  if (shortForm) {
    sprintf(out, "%02X:%02X..%02X", m[0], m[1], m[5]);
  } else {
    sprintf(out, "%02X:%02X:%02X:%02X:%02X:%02X",
            m[0], m[1], m[2], m[3], m[4], m[5]);
  }
}

static const char* vendorOf(const uint8_t* m) {
  char pfx[9];
  sprintf(pfx, "%02X:%02X:%02X", m[0], m[1], m[2]);
  for (int i = 0; i < OUI_COUNT; i++) {
    if (strcasecmp(pfx, ouiTable[i].prefix) == 0) return ouiTable[i].name;
  }
  return nullptr;
}

static float scorePersist(const Tracked& d, uint32_t now) {
  uint32_t age = now - d.firstSeen;
  if (age < 1000) age = 1000;
  float density = (float)d.hits / ((float)age / 1000.0f);
  float recency = 1.0f - min(1.0f, (float)(now - d.lastSeen) / (float)LEFT_AFTER_MS);
  float s = (0.55f * min(1.0f, density / 0.4f)) + (0.45f * recency);
  if (s < 0) s = 0;
  if (s > 1) s = 1;
  return s;
}

static int findMac(const uint8_t* mac) {
  for (int i = 0; i < MAX_DEVICES; i++) {
    if (g_dev[i].used && macEq(g_dev[i].mac, mac)) return i;
  }
  return -1;
}

static int allocSlot() {
  for (int i = 0; i < MAX_DEVICES; i++) {
    if (!g_dev[i].used) return i;
  }
  int oldest = 0;
  uint32_t oldestT = UINT32_MAX;
  for (int i = 0; i < MAX_DEVICES; i++) {
    if (g_dev[i].lastSeen < oldestT) {
      oldestT = g_dev[i].lastSeen;
      oldest = i;
    }
  }
  return oldest;
}

static void upsert(const uint8_t* mac, const char* name, int8_t rssi, RadioKind kind) {
  uint32_t now = millis();
  int idx = findMac(mac);
  if (idx < 0) {
    idx = allocSlot();
    memset(&g_dev[idx], 0, sizeof(Tracked));
    memcpy(g_dev[idx].mac, mac, 6);
    g_dev[idx].firstSeen = now;
    g_dev[idx].hits = 0;
    g_dev[idx].used = true;
  }
  g_dev[idx].rssi = rssi;
  g_dev[idx].kind |= (uint8_t)kind;
  g_dev[idx].hits++;
  g_dev[idx].lastSeen = now;
  g_dev[idx].left = false;
  if (name && name[0]) {
    strncpy(g_dev[idx].name, name, sizeof(g_dev[idx].name) - 1);
    g_dev[idx].name[sizeof(g_dev[idx].name) - 1] = 0;
  } else if (!g_dev[idx].name[0]) {
    const char* v = vendorOf(mac);
    if (v) strncpy(g_dev[idx].name, v, sizeof(g_dev[idx].name) - 1);
  }
  g_dev[idx].persist = scorePersist(g_dev[idx], now);
}

static void markLeft() {
  uint32_t now = millis();
  g_count = 0;
  for (int i = 0; i < MAX_DEVICES; i++) {
    if (!g_dev[i].used) continue;
    g_count++;
    if (now - g_dev[i].lastSeen > LEFT_AFTER_MS) g_dev[i].left = true;
    g_dev[i].persist = scorePersist(g_dev[i], now);
  }
}

static void clearTable() {
  memset(g_dev, 0, sizeof(g_dev));
  g_count = 0;
  g_listOff = 0;
  g_listSel = 0;
  g_detailIdx = -1;
}

static void counts(int& total, int& active, int& left, int& high) {
  total = active = left = high = 0;
  for (int i = 0; i < MAX_DEVICES; i++) {
    if (!g_dev[i].used) continue;
    total++;
    if (g_dev[i].left) left++;
    else active++;
    if (g_dev[i].persist >= HIGH_PERSIST) high++;
  }
}

static int packedIndex(int n, bool alertsOnly) {
  int seen = 0;
  for (int i = 0; i < MAX_DEVICES; i++) {
    if (!g_dev[i].used) continue;
    bool keep = alertsOnly ? (g_dev[i].left || g_dev[i].persist >= HIGH_PERSIST) : true;
    if (!keep) continue;
    if (seen == n) return i;
    seen++;
  }
  return -1;
}

static int packedCount(bool alertsOnly) {
  int n = 0;
  for (int i = 0; i < MAX_DEVICES; i++) {
    if (!g_dev[i].used) continue;
    if (alertsOnly) {
      if (g_dev[i].left || g_dev[i].persist >= HIGH_PERSIST) n++;
    } else n++;
  }
  return n;
}

static void loadPrefs() {
  g_prefs.begin("wifighter", true);
  g_scanning = g_prefs.getBool("scan", true);
  g_wifiOn = g_prefs.getBool("wifi", true);
  g_bleOn = g_prefs.getBool("ble", true);
  LEFT_AFTER_MS = g_prefs.getULong("left", 45000);
  SCAN_PERIOD_MS = g_prefs.getULong("period", 5000);
  g_prefs.end();
}

static void savePrefs() {
  g_prefs.begin("wifighter", false);
  g_prefs.putBool("scan", g_scanning);
  g_prefs.putBool("wifi", g_wifiOn);
  g_prefs.putBool("ble", g_bleOn);
  g_prefs.putULong("left", LEFT_AFTER_MS);
  g_prefs.putULong("period", SCAN_PERIOD_MS);
  g_prefs.end();
}

static void scanWifi() {
  if (!g_wifiOn) return;
  strncpy(g_status, "WiFi scan", sizeof(g_status) - 1);
  int n = WiFi.scanNetworks(false, true);
  for (int i = 0; i < n; i++) {
    uint8_t mac[6];
    memcpy(mac, WiFi.BSSID(i), 6);
    String ssid = WiFi.SSID(i);
    char nm[18] = {0};
    if (ssid.length()) strncpy(nm, ssid.c_str(), 17);
    upsert(mac, nm, (int8_t)WiFi.RSSI(i), KIND_WIFI);
  }
  WiFi.scanDelete();
}

static void scanBle() {
  if (!g_bleOn) return;
  strncpy(g_status, "BLE scan", sizeof(g_status) - 1);
  NimBLEScan* scan = NimBLEDevice::getScan();
  if (!scan) return;
  scan->setActiveScan(true);
  scan->setInterval(134);
  scan->setWindow(120);
  NimBLEScanResults res = scan->start(BLE_SCAN_MS / 1000, false);
  int n = res.getCount();
  for (int i = 0; i < n; i++) {
    const NimBLEAdvertisedDevice* d = res.getDevice(i);
    if (!d) continue;
    NimBLEAddress addr = d->getAddress();
    const uint8_t* raw = addr.getNative();
    uint8_t mac[6];
    memcpy(mac, raw, 6);
    char nm[18] = {0};
    if (d->haveName()) {
      String name = d->getName();
      strncpy(nm, name.c_str(), 17);
    }
    upsert(mac, nm, (int8_t)d->getRSSI(), KIND_BLE);
  }
  scan->clearResults();
}

static void runScanCycle() {
  if (!g_scanning || g_scanBusy) return;
  uint32_t now = millis();
  if (now - g_lastScan < SCAN_PERIOD_MS) return;
  g_scanBusy = true;
  g_lastScan = now;
  scanWifi();
  scanBle();
  markLeft();
  strncpy(g_status, "watching", sizeof(g_status) - 1);
  g_scanBusy = false;

  int total, active, left, high;
  counts(total, active, left, high);
  Serial.printf("[WF] v%s live=%d left=%d high=%d total=%d wifi=%d ble=%d\n",
                FW_VERSION, active, left, high, total, g_wifiOn, g_bleOn);
}

static int batPct() {
  int p = M5.Power.getBatteryLevel();
  if (p < 0) p = 0;
  if (p > 100) p = 100;
  return p;
}

static void headerBar(const char* title) {
  auto& d = M5.Display;
  d.fillRect(0, 0, d.width(), 16, COL_PANEL);
  d.setTextDatum(middle_left);
  d.setTextColor(COL_ACCENT, COL_PANEL);
  d.setTextSize(1);
  d.drawString(title, 4, 8);
  d.setTextDatum(middle_right);
  d.setTextColor(COL_DIM, COL_PANEL);
  char bat[16];
  sprintf(bat, "%s %d%%", g_scanning ? "ON" : "OFF", batPct());
  d.drawString(bat, d.width() - 4, 8);
}

static void footer(const char* left, const char* right) {
  auto& d = M5.Display;
  int y = d.height() - 14;
  d.fillRect(0, y, d.width(), 14, COL_PANEL);
  d.setTextDatum(middle_left);
  d.setTextColor(COL_DIM, COL_PANEL);
  d.setTextSize(1);
  d.drawString(left, 4, y + 7);
  d.setTextDatum(middle_right);
  d.drawString(right, d.width() - 4, y + 7);
}

static void drawSplash() {
  auto& d = M5.Display;
  d.fillScreen(COL_BG);
  d.setTextDatum(middle_center);
  d.setTextColor(COL_ACCENT, COL_BG);
  d.setTextSize(2);
  d.drawString("WiFighter", d.width() / 2, 48);
  d.setTextSize(1);
  d.setTextColor(COL_TEXT, COL_BG);
  d.drawString("leftover RF awareness", d.width() / 2, 74);
  d.setTextColor(COL_WARN, COL_BG);
  d.drawString("authorized use only", d.width() / 2, 92);
  delay(800);
}

static void drawHome() {
  auto& d = M5.Display;
  d.fillScreen(COL_BG);
  headerBar("WiFighter  HOME");

  int total, active, left, high;
  counts(total, active, left, high);

  int cx = 28, cy = 48;
  uint16_t ring = g_scanning ? COL_OK : COL_MUTED;
  if (high > 0) ring = COL_WARN;
  if (left > 0) ring = COL_BAD;
  d.fillCircle(cx, cy, 20, ring);
  d.fillCircle(cx, cy, 16, COL_BG);
  d.setTextDatum(middle_center);
  d.setTextColor(COL_TEXT, COL_BG);
  d.setTextSize(1);
  d.drawString(g_scanning ? "ON" : "OFF", cx, cy);

  d.setTextDatum(top_left);
  d.setTextColor(COL_ACCENT, COL_BG);
  d.setCursor(56, 20);
  d.printf("v%s", FW_VERSION);
  d.setTextColor(COL_DIM, COL_BG);
  d.setCursor(56, 32);
  d.printf("%s", g_status);
  d.setCursor(56, 44);
  d.printf("W:%s  B:%s", g_wifiOn ? "Y" : "N", g_bleOn ? "Y" : "N");
  d.setCursor(56, 56);
  d.printf("left>%lus", (unsigned long)(LEFT_AFTER_MS / 1000));

  auto stat = [&](int x, int y, const char* lab, int val, uint16_t c) {
    d.fillRoundRect(x, y, 54, 28, 3, COL_PANEL);
    d.setTextDatum(top_center);
    d.setTextColor(COL_MUTED, COL_PANEL);
    d.drawString(lab, x + 27, y + 3);
    d.setTextColor(c, COL_PANEL);
    char buf[8];
    sprintf(buf, "%d", val);
    d.drawString(buf, x + 27, y + 14);
  };

  stat(8, 80, "DEV", total, COL_TEXT);
  stat(66, 80, "LIVE", active, COL_OK);
  stat(124, 80, "LEFT", left, left ? COL_BAD : COL_DIM);
  stat(182, 80, "HIGH", high, high ? COL_WARN : COL_DIM);

  footer("A:scan  B:menu", g_scanning ? "SCAN ON" : "SCAN OFF");
}

static void drawMenu() {
  auto& d = M5.Display;
  d.fillScreen(COL_BG);
  headerBar("Menu");
  for (int i = 0; i < MENU_N; i++) {
    int y = 18 + i * 14;
    bool sel = (i == g_menuIdx);
    uint16_t bg = sel ? COL_ACCENT : COL_BG;
    uint16_t fg = sel ? COL_BG : COL_TEXT;
    d.fillRect(0, y, d.width(), 14, bg);
    d.setTextDatum(middle_left);
    d.setTextColor(fg, bg);
    d.setTextSize(1);
    char line[36];
    if (i == 0) {
      sprintf(line, "%s  [%s]", MENU_ITEMS[i], g_scanning ? "ON" : "OFF");
    } else {
      strncpy(line, MENU_ITEMS[i], sizeof(line) - 1);
      line[sizeof(line) - 1] = 0;
    }
    d.drawString(line, 8, y + 7);
  }
  footer("A:select  B:next", "hold B:home");
}

static void drawList(bool alerts) {
  auto& d = M5.Display;
  d.fillScreen(COL_BG);
  headerBar(alerts ? "Alerts / Left" : "Devices");

  int n = packedCount(alerts);
  if (n == 0) {
    d.setTextDatum(middle_center);
    d.setTextColor(COL_MUTED, COL_BG);
    d.drawString(alerts ? "No leftovers yet" : "No devices yet", d.width() / 2, d.height() / 2);
    footer("A:back  B:scroll", "0");
    return;
  }
  if (g_listSel >= n) g_listSel = n - 1;
  if (g_listSel < 0) g_listSel = 0;
  if (g_listSel < g_listOff) g_listOff = g_listSel;
  if (g_listSel >= g_listOff + VISIBLE_ROWS) g_listOff = g_listSel - VISIBLE_ROWS + 1;

  for (int row = 0; row < VISIBLE_ROWS; row++) {
    int pi = g_listOff + row;
    if (pi >= n) break;
    int di = packedIndex(pi, alerts);
    if (di < 0) break;
    int y = 18 + row * 20;
    bool sel = (pi == g_listSel);
    uint16_t bg = sel ? COL_PANEL : COL_BG;
    d.fillRect(0, y, d.width(), 20, bg);

    char smac[16];
    macStr(g_dev[di].mac, smac, true);
    char kind = (g_dev[di].kind == KIND_BOTH) ? '*' :
                (g_dev[di].kind & KIND_BLE) ? 'B' : 'W';
    uint16_t c = COL_TEXT;
    if (g_dev[di].left) c = COL_BAD;
    else if (g_dev[di].persist >= HIGH_PERSIST) c = COL_WARN;
    else if (g_dev[di].persist >= 0.25f) c = COL_OK;

    d.setTextDatum(middle_left);
    d.setTextColor(c, bg);
    char line[40];
    const char* lab = g_dev[di].name[0] ? g_dev[di].name : smac;
    sprintf(line, "%c %s  %ddB", kind, lab, (int)g_dev[di].rssi);
    d.drawString(line, 4, y + 6);
    d.setTextColor(COL_MUTED, bg);
    char pbuf[12];
    sprintf(pbuf, "p%.2f%s", g_dev[di].persist, g_dev[di].left ? " L" : "");
    d.setTextDatum(middle_right);
    d.drawString(pbuf, d.width() - 4, y + 14);
  }
  char info[16];
  sprintf(info, "%d/%d", g_listSel + 1, n);
  footer("A:open/back  B:next", info);
}

static void drawDetail() {
  auto& d = M5.Display;
  d.fillScreen(COL_BG);
  headerBar("Device");
  if (g_detailIdx < 0 || !g_dev[g_detailIdx].used) {
    d.setTextDatum(middle_center);
    d.setTextColor(COL_MUTED, COL_BG);
    d.drawString("gone", d.width() / 2, d.height() / 2);
    footer("A:back", "");
    return;
  }
  Tracked& t = g_dev[g_detailIdx];
  char full[20];
  macStr(t.mac, full, false);
  d.setTextDatum(top_left);
  d.setTextColor(COL_ACCENT, COL_BG);
  d.setCursor(6, 20);
  d.print(full);
  d.setTextColor(COL_TEXT, COL_BG);
  d.setCursor(6, 34);
  d.printf("name %s", t.name[0] ? t.name : "-");
  const char* v = vendorOf(t.mac);
  d.setCursor(6, 46);
  d.printf("oui  %s", v ? v : "unknown");
  d.setCursor(6, 58);
  d.printf("rssi %d dBm", (int)t.rssi);
  d.setCursor(6, 70);
  const char* k = (t.kind == KIND_BOTH) ? "BLE+WiFi" :
                  (t.kind & KIND_BLE) ? "BLE" : "WiFi";
  d.printf("radio %s", k);
  d.setCursor(6, 82);
  d.printf("hits %u  persist %.2f", (unsigned)t.hits, t.persist);
  d.setCursor(6, 94);
  d.setTextColor(t.left ? COL_BAD : COL_OK, COL_BG);
  d.printf("state %s  last %lus", t.left ? "LEFT" : "HERE",
           (unsigned)((millis() - t.lastSeen) / 1000));
  footer("A:back", "");
}

static void drawSettings() {
  auto& d = M5.Display;
  d.fillScreen(COL_BG);
  headerBar("Settings");

  char lines[6][36];
  snprintf(lines[0], 36, "WiFi radio     %s", g_wifiOn ? "ON" : "OFF");
  snprintf(lines[1], 36, "BLE radio      %s", g_bleOn ? "ON" : "OFF");
  snprintf(lines[2], 36, "Scan period    %lus", (unsigned long)(SCAN_PERIOD_MS / 1000));
  snprintf(lines[3], 36, "Left after     %lus", (unsigned long)(LEFT_AFTER_MS / 1000));
  snprintf(lines[4], 36, "Save settings");
  snprintf(lines[5], 36, "Back to menu");

  for (int i = 0; i < 6; i++) {
    int y = 20 + i * 16;
    bool sel = (i == g_setIdx);
    uint16_t bg = sel ? COL_ACCENT : COL_BG;
    uint16_t fg = sel ? COL_BG : COL_TEXT;
    d.fillRect(0, y, d.width(), 16, bg);
    d.setTextDatum(middle_left);
    d.setTextColor(fg, bg);
    d.drawString(lines[i], 8, y + 8);
  }
  footer("A:toggle  B:next", "NVS");
}

static void drawAbout() {
  auto& d = M5.Display;
  d.fillScreen(COL_BG);
  headerBar("About");
  d.setTextDatum(top_left);
  d.setTextColor(COL_ACCENT, COL_BG);
  d.setCursor(6, 22);
  d.print("WiFighter");
  d.setTextColor(COL_TEXT, COL_BG);
  d.setCursor(6, 36);
  d.printf("firmware %s", FW_VERSION);
  d.setCursor(6, 50);
  d.print("M5StickC Plus / Plus2");
  d.setCursor(6, 64);
  d.print("BLE + WiFi leftover track");
  d.setCursor(6, 78);
  d.setTextColor(COL_WARN, COL_BG);
  d.print("authorized use only");
  d.setCursor(6, 96);
  d.setTextColor(COL_MUTED, COL_BG);
  d.printf("up %lus  slots %d", (unsigned)((millis() - g_bootMs) / 1000), MAX_DEVICES);
  footer("A:back", "MIT");
}

static void render() {
  switch (g_screen) {
    case SCR_HOME:     drawHome(); break;
    case SCR_MENU:     drawMenu(); break;
    case SCR_DEVICES:  drawList(false); break;
    case SCR_ALERTS:   drawList(true); break;
    case SCR_SETTINGS: drawSettings(); break;
    case SCR_ABOUT:    drawAbout(); break;
    case SCR_DETAIL:   drawDetail(); break;
  }
}

static void openList(bool alerts) {
  g_listOff = 0;
  g_listSel = 0;
  g_screen = alerts ? SCR_ALERTS : SCR_DEVICES;
}

static void handleA() {
  switch (g_screen) {
    case SCR_HOME:
      g_scanning = !g_scanning;
      strncpy(g_status, g_scanning ? "armed" : "paused", sizeof(g_status) - 1);
      break;
    case SCR_MENU:
      switch (g_menuIdx) {
        case 0: g_scanning = !g_scanning; break;
        case 1: openList(false); break;
        case 2: openList(true); break;
        case 3: g_screen = SCR_SETTINGS; g_setIdx = 0; break;
        case 4: clearTable(); strncpy(g_status, "cleared", sizeof(g_status) - 1); g_screen = SCR_HOME; break;
        case 5: g_screen = SCR_ABOUT; break;
        default: g_screen = SCR_HOME; break;
      }
      break;
    case SCR_DEVICES:
    case SCR_ALERTS: {
      int n = packedCount(g_screen == SCR_ALERTS);
      if (n == 0) { g_screen = SCR_MENU; break; }
      int di = packedIndex(g_listSel, g_screen == SCR_ALERTS);
      if (di >= 0) {
        g_detailIdx = di;
        g_screen = SCR_DETAIL;
      }
      break;
    }
    case SCR_DETAIL:
      g_screen = (g_detailIdx >= 0 && (g_dev[g_detailIdx].left || g_dev[g_detailIdx].persist >= HIGH_PERSIST))
                 ? SCR_ALERTS : SCR_DEVICES;
      break;
    case SCR_SETTINGS:
      switch (g_setIdx) {
        case 0: g_wifiOn = !g_wifiOn; break;
        case 1: g_bleOn = !g_bleOn; break;
        case 2:
          SCAN_PERIOD_MS = (SCAN_PERIOD_MS == 3000) ? 5000 :
                           (SCAN_PERIOD_MS == 5000) ? 8000 : 3000;
          break;
        case 3:
          LEFT_AFTER_MS = (LEFT_AFTER_MS == 30000) ? 45000 :
                          (LEFT_AFTER_MS == 45000) ? 90000 : 30000;
          break;
        case 4: savePrefs(); g_screen = SCR_MENU; break;
        default: g_screen = SCR_MENU; break;
      }
      break;
    case SCR_ABOUT:
      g_screen = SCR_MENU;
      break;
  }
}

static void handleB() {
  switch (g_screen) {
    case SCR_HOME:
      g_screen = SCR_MENU;
      g_menuIdx = 0;
      break;
    case SCR_MENU:
      g_menuIdx = (g_menuIdx + 1) % MENU_N;
      break;
    case SCR_DEVICES:
    case SCR_ALERTS: {
      int n = packedCount(g_screen == SCR_ALERTS);
      if (n > 0) g_listSel = (g_listSel + 1) % n;
      break;
    }
    case SCR_SETTINGS:
      g_setIdx = (g_setIdx + 1) % 6;
      break;
    default:
      g_screen = SCR_MENU;
      break;
  }
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(1);
  M5.Display.setBrightness(80);
  g_bootMs = millis();
  loadPrefs();
  drawSplash();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(80);

  NimBLEDevice::init("");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  strncpy(g_status, "boot", sizeof(g_status) - 1);
  drawHome();
}

void loop() {
  M5.update();
  bool dirty = false;

  if (M5.BtnB.isPressed()) {
    if (g_btnBDown == 0) g_btnBDown = millis();
    if (!g_holdConsumed && millis() - g_btnBDown > 650 && g_screen != SCR_HOME) {
      g_screen = SCR_HOME;
      g_holdConsumed = true;
      dirty = true;
    }
  } else {
    if (g_btnBDown && !g_holdConsumed && M5.BtnB.wasReleased()) {
      handleB();
      dirty = true;
    }
    g_btnBDown = 0;
    g_holdConsumed = false;
  }

  if (M5.BtnA.wasPressed()) { handleA(); dirty = true; }

  runScanCycle();

  static uint32_t lastPaint = 0;
  if (dirty || millis() - lastPaint > 400) {
    render();
    lastPaint = millis();
  }
  delay(15);
}
