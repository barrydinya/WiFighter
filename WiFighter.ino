/*
 * WiFighter — M5StickC Plus / Plus2
 * Passive BLE + WiFi leftover tracker
 *
 * Authorized security research / personal privacy monitoring only.
 * Unauthorized tracking or disruption is illegal.
 *
 * What it collects (receive-only):
 *   - Wi-Fi AP beacons via scanNetworks
 *   - Wi-Fi probe requests from stations that are not associated
 *     (devices that left an AP but still advertise remembered SSIDs)
 *   - BLE advertisements, including common tracker company IDs
 *
 * Controls
 *   BtnB       open Menu / next item / scroll
 *   BtnA       select / enter / back / toggle scan on Home
 *   Hold BtnB  jump Home from any screen
 */

#include <M5Unified.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include "oui.h"

#define FW_VERSION "0.5.0"

static uint32_t SCAN_PERIOD_MS = 6000;
static uint32_t BLE_SCAN_MS    = 2200;
static uint32_t LEFT_AFTER_MS  = 45000;
static const float HIGH_PERSIST = 0.45f;
static const int   MAX_DEVICES  = 64;
static const int   VISIBLE_ROWS = 5;
static const int   MAX_ALLOW    = 6;
static const int   PROBE_SLOTS  = 20;

enum RadioKind : uint8_t { KIND_BLE = 1, KIND_WIFI = 2, KIND_BOTH = 3, KIND_PROBE = 4 };
enum Screen : uint8_t {
  SCR_HOME, SCR_MENU, SCR_DEVICES, SCR_ALERTS, SCR_SETTINGS, SCR_ABOUT, SCR_DETAIL
};
enum TagKind : uint8_t { TAG_NONE = 0, TAG_APPLE = 1, TAG_TILE = 2, TAG_SAMSUNG = 3, TAG_GOOGLE = 4 };

struct Tracked {
  uint8_t  mac[6];
  char     name[18];
  char     ssid[18];
  int8_t   rssi;
  uint8_t  kind;
  uint8_t  tag;
  uint16_t hits;
  uint32_t firstSeen;
  uint32_t lastSeen;
  float    persist;
  bool     left;
  bool     probe;
  bool     allow;
  bool     used;
};

struct ProbeHit {
  uint8_t mac[6];
  int8_t  rssi;
  char    ssid[18];
  bool    used;
};

static Tracked g_dev[MAX_DEVICES];
static int g_count = 0;
static ProbeHit g_probes[PROBE_SLOTS];
static portMUX_TYPE g_probeMux = portMUX_INITIALIZER_UNLOCKED;

static Screen g_screen = SCR_HOME;
static int g_menuIdx = 0;
static int g_listOff = 0;
static int g_listSel = 0;
static int g_detailIdx = -1;
static int g_setIdx = 0;

static bool g_scanning = true;
static bool g_wifiOn = true;
static bool g_bleOn = true;
static bool g_probesOn = true;
static bool g_scanBusy = false;
static uint32_t g_lastScan = 0;
static uint32_t g_bootMs = 0;
static uint32_t g_btnBDown = 0;
static bool g_holdConsumed = false;
static char g_status[24] = "idle";
static char g_lastLeft[22] = "none yet";
static int g_probeCount = 0;
static Preferences g_prefs;
static uint8_t g_allow[MAX_ALLOW][6];
static int g_allowN = 0;

static const char* MENU_ITEMS[] = {
  "Start / Stop scan",
  "Device list",
  "Left / alerts",
  "Settings",
  "Allow this MAC",
  "Clear table",
  "About",
  "Home"
};
static const int MENU_N = 8;

static uint16_t COL_BG     = 0x0841;
static uint16_t COL_PANEL  = 0x1082;
static uint16_t COL_ACCENT = 0x07FF;
static uint16_t COL_OK     = 0x07E0;
static uint16_t COL_WARN   = 0xFD20;
static uint16_t COL_BAD    = 0xF800;
static uint16_t COL_MUTED  = 0x8410;
static uint16_t COL_TEXT   = 0xFFFF;
static uint16_t COL_DIM    = 0xC618;

static bool macEq(const uint8_t* a, const uint8_t* b) {
  return memcmp(a, b, 6) == 0;
}

static bool isAllow(const uint8_t* mac) {
  for (int i = 0; i < g_allowN; i++) {
    if (macEq(g_allow[i], mac)) return true;
  }
  return false;
}

static void macStr(const uint8_t* m, char* out, bool shortForm) {
  if (shortForm) sprintf(out, "%02X:%02X..%02X", m[0], m[1], m[5]);
  else sprintf(out, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static const char* vendorOf(const uint8_t* m) {
  char pfx[9];
  sprintf(pfx, "%02X:%02X:%02X", m[0], m[1], m[2]);
  for (int i = 0; i < OUI_COUNT; i++) {
    if (strcasecmp(pfx, ouiTable[i].prefix) == 0) return ouiTable[i].name;
  }
  return nullptr;
}

static const char* tagName(uint8_t t) {
  switch (t) {
    case TAG_APPLE: return "Apple";
    case TAG_TILE: return "Tile";
    case TAG_SAMSUNG: return "Samsung";
    case TAG_GOOGLE: return "Google";
    default: return "";
  }
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
  for (int i = 0; i < MAX_DEVICES; i++) if (!g_dev[i].used) return i;
  int oldest = 0;
  uint32_t oldestT = UINT32_MAX;
  for (int i = 0; i < MAX_DEVICES; i++) {
    if (g_dev[i].allow) continue;
    if (g_dev[i].lastSeen < oldestT) {
      oldestT = g_dev[i].lastSeen;
      oldest = i;
    }
  }
  return oldest;
}

static void noteLeft(const Tracked& d) {
  const char* lab = d.name[0] ? d.name : (d.ssid[0] ? d.ssid : "unknown");
  snprintf(g_lastLeft, sizeof(g_lastLeft), "%s", lab);
}

static void upsert(const uint8_t* mac, const char* name, int8_t rssi, uint8_t kind, const char* ssid, uint8_t tag) {
  if (isAllow(mac)) return;
  uint32_t now = millis();
  int idx = findMac(mac);
  bool wasLeft = false;
  if (idx < 0) {
    idx = allocSlot();
    memset(&g_dev[idx], 0, sizeof(Tracked));
    memcpy(g_dev[idx].mac, mac, 6);
    g_dev[idx].firstSeen = now;
    g_dev[idx].used = true;
  } else {
    wasLeft = g_dev[idx].left;
  }
  g_dev[idx].rssi = rssi;
  g_dev[idx].kind = (uint8_t)(g_dev[idx].kind | kind);
  g_dev[idx].hits++;
  g_dev[idx].lastSeen = now;
  g_dev[idx].left = false;
  if (kind & KIND_PROBE) g_dev[idx].probe = true;
  if (tag) g_dev[idx].tag = tag;
  if (name && name[0]) {
    strncpy(g_dev[idx].name, name, sizeof(g_dev[idx].name) - 1);
  } else if (!g_dev[idx].name[0]) {
    const char* v = vendorOf(mac);
    if (v) strncpy(g_dev[idx].name, v, sizeof(g_dev[idx].name) - 1);
    else if (tag) strncpy(g_dev[idx].name, tagName(tag), sizeof(g_dev[idx].name) - 1);
  }
  if (ssid && ssid[0]) strncpy(g_dev[idx].ssid, ssid, sizeof(g_dev[idx].ssid) - 1);
  g_dev[idx].persist = scorePersist(g_dev[idx], now);
  if (wasLeft) noteLeft(g_dev[idx]);
}

static void markLeft() {
  uint32_t now = millis();
  g_count = 0;
  for (int i = 0; i < MAX_DEVICES; i++) {
    if (!g_dev[i].used) continue;
    g_count++;
    bool nowLeft = (now - g_dev[i].lastSeen > LEFT_AFTER_MS);
    if (nowLeft && !g_dev[i].left) noteLeft(g_dev[i]);
    g_dev[i].left = nowLeft;
    g_dev[i].persist = scorePersist(g_dev[i], now);
  }
}

static void clearTable() {
  memset(g_dev, 0, sizeof(g_dev));
  g_count = 0;
  g_listOff = 0;
  g_listSel = 0;
  g_detailIdx = -1;
  strncpy(g_lastLeft, "cleared", sizeof(g_lastLeft) - 1);
}

static void counts(int& total, int& active, int& left, int& high, int& probes) {
  total = active = left = high = probes = 0;
  for (int i = 0; i < MAX_DEVICES; i++) {
    if (!g_dev[i].used) continue;
    total++;
    if (g_dev[i].left) left++;
    else active++;
    if (g_dev[i].persist >= HIGH_PERSIST) high++;
    if (g_dev[i].probe) probes++;
  }
}

static int packedIndex(int n, bool alertsOnly) {
  int seen = 0;
  for (int i = 0; i < MAX_DEVICES; i++) {
    if (!g_dev[i].used) continue;
    bool keep = alertsOnly ? (g_dev[i].left || g_dev[i].probe || g_dev[i].persist >= HIGH_PERSIST || g_dev[i].tag)
                           : true;
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
    if (!alertsOnly || g_dev[i].left || g_dev[i].probe || g_dev[i].persist >= HIGH_PERSIST || g_dev[i].tag) n++;
  }
  return n;
}

static void loadPrefs() {
  g_prefs.begin("wifighter", true);
  g_scanning = g_prefs.getBool("scan", true);
  g_wifiOn = g_prefs.getBool("wifi", true);
  g_bleOn = g_prefs.getBool("ble", true);
  g_probesOn = g_prefs.getBool("probe", true);
  LEFT_AFTER_MS = g_prefs.getULong("left", 45000);
  SCAN_PERIOD_MS = g_prefs.getULong("period", 6000);
  g_allowN = g_prefs.getInt("an", 0);
  if (g_allowN < 0 || g_allowN > MAX_ALLOW) g_allowN = 0;
  g_prefs.getBytes("allow", g_allow, sizeof(g_allow));
  g_prefs.end();
}

static void savePrefs() {
  g_prefs.begin("wifighter", false);
  g_prefs.putBool("scan", g_scanning);
  g_prefs.putBool("wifi", g_wifiOn);
  g_prefs.putBool("ble", g_bleOn);
  g_prefs.putBool("probe", g_probesOn);
  g_prefs.putULong("left", LEFT_AFTER_MS);
  g_prefs.putULong("period", SCAN_PERIOD_MS);
  g_prefs.putInt("an", g_allowN);
  g_prefs.putBytes("allow", g_allow, sizeof(g_allow));
  g_prefs.end();
}

static void addAllow(const uint8_t* mac) {
  if (isAllow(mac) || g_allowN >= MAX_ALLOW) return;
  memcpy(g_allow[g_allowN], mac, 6);
  g_allowN++;
  int idx = findMac(mac);
  if (idx >= 0) g_dev[idx].used = false;
  savePrefs();
}

static void IRAM_ATTR sniffCb(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  const wifi_promiscuous_pkt_t* pp = (const wifi_promiscuous_pkt_t*)buf;
  const uint8_t* p = pp->payload;
  uint16_t len = pp->rx_ctrl.sig_len;
  if (len < 26) return;
  uint8_t subtype = (p[0] >> 4) & 0x0F;
  if (subtype != 0x04) return;
  const uint8_t* sa = p + 10;
  if ((sa[0] & 0x01) != 0) return;
  char ssid[18] = {0};
  uint16_t i = 24;
  while (i + 2 < len) {
    uint8_t id = p[i];
    uint8_t l = p[i + 1];
    if ((uint16_t)(i + 2 + l) > len) break;
    if (id == 0 && l > 0 && l < 18) {
      memcpy(ssid, p + i + 2, l);
      ssid[l] = 0;
      break;
    }
    if (id == 0) break;
    i = (uint16_t)(i + 2 + l);
  }
  portENTER_CRITICAL_ISR(&g_probeMux);
  int slot = -1;
  for (int n = 0; n < PROBE_SLOTS; n++) {
    if (g_probes[n].used && memcmp(g_probes[n].mac, sa, 6) == 0) { slot = n; break; }
    if (!g_probes[n].used && slot < 0) slot = n;
  }
  if (slot >= 0) {
    memcpy(g_probes[slot].mac, sa, 6);
    g_probes[slot].rssi = (int8_t)pp->rx_ctrl.rssi;
    if (ssid[0]) memcpy(g_probes[slot].ssid, ssid, 18);
    g_probes[slot].used = true;
  }
  portEXIT_CRITICAL_ISR(&g_probeMux);
}

static void scanProbes() {
  if (!g_wifiOn || !g_probesOn) return;
  strncpy(g_status, "probe hop", sizeof(g_status) - 1);
  memset(g_probes, 0, sizeof(g_probes));
  wifi_promiscuous_filter_t filt = {};
  filt.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
  esp_wifi_set_promiscuous_filter(&filt);
  esp_wifi_set_promiscuous_rx_cb(sniffCb);
  esp_wifi_set_promiscuous(true);
  const uint8_t chans[] = {1, 6, 11};
  for (uint8_t c : chans) {
    esp_wifi_set_channel(c, WIFI_SECOND_CHAN_NONE);
    delay(280);
  }
  esp_wifi_set_promiscuous(false);
  ProbeHit local[PROBE_SLOTS];
  portENTER_CRITICAL(&g_probeMux);
  memcpy(local, g_probes, sizeof(local));
  portEXIT_CRITICAL(&g_probeMux);
  g_probeCount = 0;
  for (int i = 0; i < PROBE_SLOTS; i++) {
    if (!local[i].used) continue;
    g_probeCount++;
    upsert(local[i].mac, local[i].ssid, local[i].rssi, KIND_PROBE, local[i].ssid, TAG_NONE);
  }
}

static void scanWifi() {
  if (!g_wifiOn) return;
  strncpy(g_status, "WiFi APs", sizeof(g_status) - 1);
  int n = WiFi.scanNetworks(false, true);
  for (int i = 0; i < n; i++) {
    uint8_t mac[6];
    memcpy(mac, WiFi.BSSID(i), 6);
    String ssid = WiFi.SSID(i);
    char nm[18] = {0};
    if (ssid.length()) strncpy(nm, ssid.c_str(), 17);
    upsert(mac, nm, (int8_t)WiFi.RSSI(i), KIND_WIFI, nm, TAG_NONE);
  }
  WiFi.scanDelete();
}

static uint8_t bleTag(const NimBLEAdvertisedDevice* d, char* nameOut) {
  std::string payload = d->getPayload();
  const uint8_t* p = (const uint8_t*)payload.data();
  size_t n = payload.size();
  uint8_t tag = TAG_NONE;
  size_t i = 0;
  while (i + 2 < n) {
    uint8_t len = p[i];
    if (len == 0 || i + 1 + len > n) break;
    uint8_t typ = p[i + 1];
    if ((typ == 0xFF) && len >= 3) {
      uint16_t cid = p[i + 2] | (p[i + 3] << 8);
      if (cid == 0x004C) tag = TAG_APPLE;
      else if (cid == 0x0075) tag = TAG_SAMSUNG;
      else if (cid == 0x00E0 || cid == 0xFE9A) tag = TAG_GOOGLE;
    }
    i += 1 + len;
  }
  if (d->haveName()) {
    String name = d->getName();
    strncpy(nameOut, name.c_str(), 17);
    if (name.startsWith("Tile")) tag = TAG_TILE;
  }
  return tag;
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
    uint8_t mac[6];
    memcpy(mac, addr.getNative(), 6);
    char nm[18] = {0};
    uint8_t tag = bleTag(d, nm);
    upsert(mac, nm, (int8_t)d->getRSSI(), KIND_BLE, nullptr, tag);
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
  scanProbes();
  scanBle();
  markLeft();
  strncpy(g_status, "watching", sizeof(g_status) - 1);
  g_scanBusy = false;
  int total, active, left, high, probes;
  counts(total, active, left, high, probes);
  Serial.printf("[WF] v%s live=%d left=%d high=%d probe=%d total=%d\n",
                FW_VERSION, active, left, high, probes, total);
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
  char bat[18];
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
  d.drawString("WiFighter", d.width() / 2, 42);
  d.setTextSize(1);
  d.setTextColor(COL_TEXT, COL_BG);
  d.drawString("leftover RF  v" FW_VERSION, d.width() / 2, 68);
  d.setTextColor(COL_WARN, COL_BG);
  d.drawString("authorized use only", d.width() / 2, 86);
  delay(700);
}

static void drawHome() {
  auto& d = M5.Display;
  d.fillScreen(COL_BG);
  headerBar("WiFighter");

  int total, active, left, high, probes;
  counts(total, active, left, high, probes);

  int cx = 30, cy = 46;
  uint16_t ring = g_scanning ? COL_OK : COL_MUTED;
  if (high > 0 || probes > 0) ring = COL_WARN;
  if (left > 0) ring = COL_BAD;
  d.drawCircle(cx, cy, 18, ring);
  d.drawCircle(cx, cy, 12, ring);
  d.fillCircle(cx, cy, 3, ring);
  d.setTextDatum(top_center);
  d.setTextColor(ring, COL_BG);
  d.drawString(g_scanning ? "LIVE" : "HOLD", cx, cy + 16);

  d.setTextDatum(top_left);
  d.setTextColor(COL_TEXT, COL_BG);
  d.setCursor(58, 20);
  d.printf("v%s  %s", FW_VERSION, g_status);
  d.setTextColor(COL_DIM, COL_BG);
  d.setCursor(58, 34);
  d.printf("WiFi %s   BLE %s", g_wifiOn ? "ON" : "off", g_bleOn ? "ON" : "off");
  d.setCursor(58, 46);
  d.printf("probes %s   left>%lus", g_probesOn ? "ON" : "off", (unsigned long)(LEFT_AFTER_MS / 1000));
  d.setCursor(58, 58);
  d.setTextColor(COL_WARN, COL_BG);
  d.printf("last left: %s", g_lastLeft);

  auto tile = [&](int x, const char* lab, int val, uint16_t c) {
    d.fillRoundRect(x, 78, 44, 28, 3, COL_PANEL);
    d.setTextDatum(top_center);
    d.setTextColor(COL_MUTED, COL_PANEL);
    d.drawString(lab, x + 22, 81);
    d.setTextColor(c, COL_PANEL);
    char buf[8];
    sprintf(buf, "%d", val);
    d.drawString(buf, x + 22, 93);
  };
  tile(4, "DEV", total, COL_TEXT);
  tile(50, "LIVE", active, COL_OK);
  tile(96, "LEFT", left, left ? COL_BAD : COL_DIM);
  tile(142, "HIGH", high, high ? COL_WARN : COL_DIM);
  tile(188, "PRB", probes, probes ? COL_ACCENT : COL_DIM);

  footer("A scan    B menu", g_scanning ? "WATCH" : "PAUSED");
}

static void drawMenu() {
  auto& d = M5.Display;
  d.fillScreen(COL_BG);
  headerBar("Menu");
  int start = 0;
  if (g_menuIdx > 5) start = g_menuIdx - 5;
  for (int row = 0; row < 6; row++) {
    int i = start + row;
    if (i >= MENU_N) break;
    int y = 18 + row * 16;
    bool sel = (i == g_menuIdx);
    uint16_t bg = sel ? COL_ACCENT : COL_BG;
    uint16_t fg = sel ? COL_BG : COL_TEXT;
    d.fillRect(2, y, d.width() - 4, 15, bg);
    d.setTextDatum(middle_left);
    d.setTextColor(fg, bg);
    char line[40];
    if (i == 0) snprintf(line, sizeof(line), "%s  [%s]", MENU_ITEMS[i], g_scanning ? "ON" : "OFF");
    else snprintf(line, sizeof(line), "%s", MENU_ITEMS[i]);
    d.drawString(sel ? ">" : " ", 6, y + 7);
    d.drawString(line, 16, y + 7);
  }
  footer("A select   B next", "hold B home");
}

static void drawList(bool alerts) {
  auto& d = M5.Display;
  d.fillScreen(COL_BG);
  headerBar(alerts ? "Left / alerts" : "Devices");
  int n = packedCount(alerts);
  if (n == 0) {
    d.setTextDatum(middle_center);
    d.setTextColor(COL_MUTED, COL_BG);
    d.drawString(alerts ? "No leftovers yet" : "No devices yet", d.width() / 2, 64);
    footer("A back   B scroll", "0");
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
    char kind = (g_dev[di].kind & KIND_PROBE) ? 'P' :
                (g_dev[di].kind == KIND_BOTH) ? '*' :
                (g_dev[di].kind & KIND_BLE) ? 'B' : 'W';
    uint16_t c = COL_TEXT;
    if (g_dev[di].left) c = COL_BAD;
    else if (g_dev[di].tag || g_dev[di].probe) c = COL_WARN;
    else if (g_dev[di].persist >= 0.25f) c = COL_OK;
    d.setTextDatum(middle_left);
    d.setTextColor(c, bg);
    const char* lab = g_dev[di].name[0] ? g_dev[di].name : (g_dev[di].ssid[0] ? g_dev[di].ssid : smac);
    char line[40];
    snprintf(line, sizeof(line), "%c %s", kind, lab);
    d.drawString(line, 4, y + 6);
    d.setTextColor(COL_MUTED, bg);
    char pbuf[16];
    snprintf(pbuf, sizeof(pbuf), "%ddB%s", (int)g_dev[di].rssi, g_dev[di].left ? " L" : "");
    d.setTextDatum(middle_right);
    d.drawString(pbuf, d.width() - 4, y + 6);
  }
  char info[16];
  sprintf(info, "%d/%d", g_listSel + 1, n);
  footer("A open   B next", info);
}

static void drawDetail() {
  auto& d = M5.Display;
  d.fillScreen(COL_BG);
  headerBar("Device");
  if (g_detailIdx < 0 || !g_dev[g_detailIdx].used) {
    d.setTextDatum(middle_center);
    d.setTextColor(COL_MUTED, COL_BG);
    d.drawString("gone", d.width() / 2, 64);
    footer("A back", "");
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
  d.setCursor(6, 46);
  d.printf("ssid %s", t.ssid[0] ? t.ssid : "-");
  const char* v = vendorOf(t.mac);
  d.setCursor(6, 58);
  d.printf("oui  %s%s%s", v ? v : "unknown", t.tag ? " / " : "", t.tag ? tagName(t.tag) : "");
  d.setCursor(6, 70);
  const char* k = (t.kind & KIND_PROBE) ? "probe (left AP)" :
                  (t.kind == KIND_BOTH) ? "BLE+WiFi" :
                  (t.kind & KIND_BLE) ? "BLE" : "WiFi";
  d.printf("%s  %d dBm", k, (int)t.rssi);
  d.setCursor(6, 82);
  d.printf("hits %u  persist %.2f", (unsigned)t.hits, t.persist);
  d.setCursor(6, 96);
  d.setTextColor(t.left ? COL_BAD : COL_OK, COL_BG);
  d.printf("%s  last %lus", t.left ? "LEFT" : "HERE", (unsigned)((millis() - t.lastSeen) / 1000));
  footer("A back", "menu to allow");
}

static void drawSettings() {
  auto& d = M5.Display;
  d.fillScreen(COL_BG);
  headerBar("Settings");
  char lines[7][36];
  snprintf(lines[0], 36, "WiFi radio      %s", g_wifiOn ? "ON" : "OFF");
  snprintf(lines[1], 36, "BLE radio       %s", g_bleOn ? "ON" : "OFF");
  snprintf(lines[2], 36, "Probe sniff     %s", g_probesOn ? "ON" : "OFF");
  snprintf(lines[3], 36, "Scan period     %lus", (unsigned long)(SCAN_PERIOD_MS / 1000));
  snprintf(lines[4], 36, "Left after      %lus", (unsigned long)(LEFT_AFTER_MS / 1000));
  snprintf(lines[5], 36, "Save settings");
  snprintf(lines[6], 36, "Back to menu");
  for (int i = 0; i < 7; i++) {
    int y = 18 + i * 14;
    bool sel = (i == g_setIdx);
    uint16_t bg = sel ? COL_ACCENT : COL_BG;
    uint16_t fg = sel ? COL_BG : COL_TEXT;
    d.fillRect(0, y, d.width(), 14, bg);
    d.setTextDatum(middle_left);
    d.setTextColor(fg, bg);
    d.drawString(lines[i], 8, y + 7);
  }
  footer("A toggle   B next", "NVS");
}

static void drawAbout() {
  auto& d = M5.Display;
  d.fillScreen(COL_BG);
  headerBar("About");
  d.setTextDatum(top_left);
  d.setTextColor(COL_ACCENT, COL_BG);
  d.setCursor(6, 22);
  d.print("WiFighter  " FW_VERSION);
  d.setTextColor(COL_TEXT, COL_BG);
  d.setCursor(6, 38);
  d.print("M5StickC Plus / Plus2");
  d.setCursor(6, 52);
  d.print("Passive leftover RF only");
  d.setCursor(6, 66);
  d.print("APs + probes + BLE ads");
  d.setCursor(6, 80);
  d.setTextColor(COL_WARN, COL_BG);
  d.print("authorized use only");
  d.setCursor(6, 96);
  d.setTextColor(COL_MUTED, COL_BG);
  d.printf("up %lus  allow %d/%d", (unsigned)((millis() - g_bootMs) / 1000), g_allowN, MAX_ALLOW);
  footer("A back", "MIT");
}

static void render() {
  switch (g_screen) {
    case SCR_HOME: drawHome(); break;
    case SCR_MENU: drawMenu(); break;
    case SCR_DEVICES: drawList(false); break;
    case SCR_ALERTS: drawList(true); break;
    case SCR_SETTINGS: drawSettings(); break;
    case SCR_ABOUT: drawAbout(); break;
    case SCR_DETAIL: drawDetail(); break;
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
        case 4:
          if (g_detailIdx >= 0 && g_dev[g_detailIdx].used) {
            addAllow(g_dev[g_detailIdx].mac);
            strncpy(g_status, "allowed", sizeof(g_status) - 1);
          } else {
            strncpy(g_status, "open a device", sizeof(g_status) - 1);
          }
          g_screen = SCR_HOME;
          break;
        case 5: clearTable(); g_screen = SCR_HOME; break;
        case 6: g_screen = SCR_ABOUT; break;
        default: g_screen = SCR_HOME; break;
      }
      break;
    case SCR_DEVICES:
    case SCR_ALERTS: {
      int n = packedCount(g_screen == SCR_ALERTS);
      if (n == 0) { g_screen = SCR_MENU; break; }
      int di = packedIndex(g_listSel, g_screen == SCR_ALERTS);
      if (di >= 0) { g_detailIdx = di; g_screen = SCR_DETAIL; }
      break;
    }
    case SCR_DETAIL:
      g_screen = (g_detailIdx >= 0 && g_dev[g_detailIdx].used &&
                  (g_dev[g_detailIdx].left || g_dev[g_detailIdx].probe))
                 ? SCR_ALERTS : SCR_DEVICES;
      break;
    case SCR_SETTINGS:
      switch (g_setIdx) {
        case 0: g_wifiOn = !g_wifiOn; break;
        case 1: g_bleOn = !g_bleOn; break;
        case 2: g_probesOn = !g_probesOn; break;
        case 3:
          SCAN_PERIOD_MS = (SCAN_PERIOD_MS == 4000) ? 6000 : (SCAN_PERIOD_MS == 6000) ? 10000 : 4000;
          break;
        case 4:
          LEFT_AFTER_MS = (LEFT_AFTER_MS == 30000) ? 45000 : (LEFT_AFTER_MS == 45000) ? 90000 : 30000;
          break;
        case 5: savePrefs(); g_screen = SCR_MENU; break;
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
      g_setIdx = (g_setIdx + 1) % 7;
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
  M5.Display.setTextSize(1);
  g_bootMs = millis();
  loadPrefs();
  drawSplash();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(80);
  esp_wifi_set_promiscuous(false);

  NimBLEDevice::init("");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  strncpy(g_status, "boot", sizeof(g_status) - 1);
  drawHome();
  Serial.printf("[WF] boot v%s\n", FW_VERSION);
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
  if (dirty || millis() - lastPaint > 500) {
    if (!g_scanBusy) render();
    lastPaint = millis();
  }
  delay(15);
}
