#pragma once
// WiFighter core — device table, leftover detection, NVS prefs.
// Passive observation only. Authorized networks/devices only.

#include <M5Unified.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include "oui.h"

#define FW_VERSION "0.8.0"
#define MAX_DEV 28
#define PROBE_SLOTS 12
#define ALLOW_SLOTS 6
#define SCAN_PERIOD_MS 8000
#define BLE_SCAN_MS 2500

enum Screen : uint8_t {
  SCR_HOME = 0,
  SCR_MENU,
  SCR_DEVICES,
  SCR_ALERTS,
  SCR_DETAIL,
  SCR_SETTINGS,
  SCR_ABOUT
};

enum Kind : uint8_t {
  KIND_WIFI = 1,
  KIND_BLE = 2,
  KIND_PROBE = 4
};

enum Tag : uint8_t {
  TAG_NONE = 0,
  TAG_APPLE,
  TAG_SAMSUNG,
  TAG_GOOGLE,
  TAG_TILE
};

enum Phase : uint8_t {
  PH_WAIT = 0,
  PH_WIFI_START,
  PH_WIFI_POLL,
  PH_PROBE_ARM,
  PH_PROBE_CH,
  PH_PROBE_DONE,
  PH_BLE,
  PH_FINISH
};

struct Dev {
  uint8_t mac[6];
  char name[18];
  char ssid[18];
  int8_t rssi;
  uint8_t kind;
  uint8_t tag;
  uint16_t hits;
  uint32_t firstMs;
  uint32_t lastMs;
  bool used;
  bool left;
};

struct ProbeHit {
  uint8_t mac[6];
  char ssid[18];
  int8_t rssi;
  bool used;
};

static Dev g_devs[MAX_DEV];
static ProbeHit g_probes[PROBE_SLOTS];
static uint8_t g_allow[ALLOW_SLOTS][6];
static uint8_t g_allowUsed = 0;

static portMUX_TYPE g_probeMux = portMUX_INITIALIZER_UNLOCKED;
static Preferences g_prefs;

static Screen g_screen = SCR_HOME;
static char g_status[16] = "boot";
static uint32_t g_bootMs = 0;
static uint32_t g_btnBDown = 0;
static bool g_holdConsumed = false;

static bool g_scanning = true;
static bool g_wifiOn = true;
static bool g_bleOn = true;
static bool g_probesOn = true;
static uint16_t g_leftWinSec = 45;
static uint16_t g_scanPeriodSec = 8;

static Phase g_phase = PH_WAIT;
static uint32_t g_lastScan = 0;
static uint32_t g_phaseT = 0;
static uint8_t g_probeCh = 0;
static uint8_t g_probeCount = 0;
static uint8_t g_hopCh = 1;

static int g_menuIdx = 0;
static int g_listIdx = 0;
static int g_listScroll = 0;
static int g_setIdx = 0;
static int g_detailIdx = -1;
static int g_lastLeft = -1;
static uint32_t g_pulse = 0;

static const char* tagName(uint8_t t) {
  switch (t) {
    case TAG_APPLE: return "Apple";
    case TAG_SAMSUNG: return "Samsung";
    case TAG_GOOGLE: return "Google";
    case TAG_TILE: return "Tile";
    default: return "";
  }
}

static const char* phaseLabel() {
  switch (g_phase) {
    case PH_WIFI_START:
    case PH_WIFI_POLL: return "AP scan";
    case PH_PROBE_ARM:
    case PH_PROBE_CH:
    case PH_PROBE_DONE: return "probes";
    case PH_BLE: return "BLE";
    case PH_FINISH: return "score";
    default: return g_scanning ? "idle" : "paused";
  }
}

static void macFmt(const uint8_t* m, char* out, size_t n) {
  snprintf(out, n, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static bool macEq(const uint8_t* a, const uint8_t* b) {
  return memcmp(a, b, 6) == 0;
}

static bool isAllowed(const uint8_t* mac) {
  for (uint8_t i = 0; i < g_allowUsed && i < ALLOW_SLOTS; i++) {
    if (macEq(g_allow[i], mac)) return true;
  }
  return false;
}

static const char* ouiLookup(const uint8_t* mac) {
  char key[9];
  snprintf(key, sizeof(key), "%02X:%02X:%02X", mac[0], mac[1], mac[2]);
  for (int i = 0; i < OUI_COUNT; i++) {
    if (strncmp(ouiTable[i].prefix, key, 8) == 0) return ouiTable[i].name;
  }
  if (mac[0] & 0x02) return "local";
  return "unknown";
}

static void saveAllow() {
  g_prefs.begin("wf", false);
  g_prefs.putUChar("alwN", g_allowUsed);
  g_prefs.putBytes("alw", g_allow, sizeof(g_allow));
  g_prefs.end();
}

static void savePrefs() {
  g_prefs.begin("wf", false);
  g_prefs.putBool("wifi", g_wifiOn);
  g_prefs.putBool("ble", g_bleOn);
  g_prefs.putBool("prb", g_probesOn);
  g_prefs.putBool("scan", g_scanning);
  g_prefs.putUShort("left", g_leftWinSec);
  g_prefs.putUShort("per", g_scanPeriodSec);
  g_prefs.putUChar("alwN", g_allowUsed);
  g_prefs.putBytes("alw", g_allow, sizeof(g_allow));
  g_prefs.end();
}

static void loadPrefs() {
  g_prefs.begin("wf", true);
  g_wifiOn = g_prefs.getBool("wifi", true);
  g_bleOn = g_prefs.getBool("ble", true);
  g_probesOn = g_prefs.getBool("prb", true);
  g_scanning = g_prefs.getBool("scan", true);
  g_leftWinSec = g_prefs.getUShort("left", 45);
  g_scanPeriodSec = g_prefs.getUShort("per", 8);
  if (g_leftWinSec < 15) g_leftWinSec = 15;
  if (g_leftWinSec > 180) g_leftWinSec = 180;
  if (g_scanPeriodSec < 4) g_scanPeriodSec = 4;
  if (g_scanPeriodSec > 60) g_scanPeriodSec = 60;
  g_allowUsed = g_prefs.getUChar("alwN", 0);
  if (g_allowUsed > ALLOW_SLOTS) g_allowUsed = 0;
  g_prefs.getBytes("alw", g_allow, sizeof(g_allow));
  g_prefs.end();
}

static int findDev(const uint8_t* mac) {
  for (int i = 0; i < MAX_DEV; i++) {
    if (g_devs[i].used && macEq(g_devs[i].mac, mac)) return i;
  }
  return -1;
}

static int allocDev() {
  int freeSlot = -1;
  int oldest = 0;
  uint32_t oldestMs = 0xFFFFFFFF;
  for (int i = 0; i < MAX_DEV; i++) {
    if (!g_devs[i].used) {
      if (freeSlot < 0) freeSlot = i;
      continue;
    }
    if (g_devs[i].lastMs < oldestMs) {
      oldestMs = g_devs[i].lastMs;
      oldest = i;
    }
  }
  return freeSlot >= 0 ? freeSlot : oldest;
}

static void upsert(const uint8_t* mac, const char* name, int8_t rssi, uint8_t kind,
                   const char* ssid, uint8_t tag) {
  if (!mac || (mac[0] & 0x01)) return;
  if (isAllowed(mac)) return;
  int i = findDev(mac);
  if (i < 0) i = allocDev();
  Dev& d = g_devs[i];
  bool fresh = !d.used || !macEq(d.mac, mac);
  if (fresh) {
    memset(&d, 0, sizeof(d));
    memcpy(d.mac, mac, 6);
    d.firstMs = millis();
    d.used = true;
  }
  d.lastMs = millis();
  d.rssi = rssi;
  d.kind = (uint8_t)(d.kind | kind);
  d.left = false;
  if (d.hits < 65000) d.hits++;
  if (name && name[0]) strncpy(d.name, name, 17);
  if (ssid && ssid[0]) strncpy(d.ssid, ssid, 17);
  if (tag != TAG_NONE) d.tag = tag;
}

static uint8_t bleTag(const NimBLEAdvertisedDevice* d, char* nm) {
  if (!d) return TAG_NONE;
  if (d->haveName()) {
    std::string n = d->getName();
    strncpy(nm, n.c_str(), 17);
    if (strcasestr(nm, "tile")) return TAG_TILE;
  }
  if (!d->haveManufacturerData()) return TAG_NONE;
  std::string md = d->getManufacturerData();
  if (md.size() < 2) return TAG_NONE;
  uint16_t cid = (uint8_t)md[0] | ((uint16_t)(uint8_t)md[1] << 8);
  if (cid == 0x004C) return TAG_APPLE;
  if (cid == 0x0075) return TAG_SAMSUNG;
  if (cid == 0x00E0) return TAG_GOOGLE;
  return TAG_NONE;
}

static void markLeft() {
  uint32_t now = millis();
  uint32_t win = (uint32_t)g_leftWinSec * 1000UL;
  int live = 0, left = 0, high = 0, probe = 0, total = 0;
  for (int i = 0; i < MAX_DEV; i++) {
    if (!g_devs[i].used) continue;
    total++;
    bool was = g_devs[i].left;
    if (now - g_devs[i].lastMs > win) {
      g_devs[i].left = true;
      left++;
      if (!was) g_lastLeft = i;
    } else {
      g_devs[i].left = false;
      live++;
    }
    if (g_devs[i].hits >= 8) high++;
    if (g_devs[i].kind & KIND_PROBE) probe++;
  }
  Serial.printf("[WF] v%s live=%d left=%d high=%d probe=%d total=%d ch=%u\n",
                FW_VERSION, live, left, high, probe, total, g_hopCh);
}

static void clearDevs() {
  memset(g_devs, 0, sizeof(g_devs));
  g_lastLeft = -1;
  g_detailIdx = -1;
}

static bool allowMac(const uint8_t* mac) {
  if (!mac || g_allowUsed >= ALLOW_SLOTS) return false;
  if (isAllowed(mac)) return true;
  memcpy(g_allow[g_allowUsed], mac, 6);
  g_allowUsed++;
  int i = findDev(mac);
  if (i >= 0) g_devs[i].used = false;
  savePrefs();
  return true;
}

static int countUsed() {
  int n = 0;
  for (int i = 0; i < MAX_DEV; i++) if (g_devs[i].used) n++;
  return n;
}

static int countLive() {
  int n = 0;
  for (int i = 0; i < MAX_DEV; i++) if (g_devs[i].used && !g_devs[i].left) n++;
  return n;
}

static int countLeft() {
  int n = 0;
  for (int i = 0; i < MAX_DEV; i++) if (g_devs[i].used && g_devs[i].left) n++;
  return n;
}

static int countHigh() {
  int n = 0;
  for (int i = 0; i < MAX_DEV; i++) if (g_devs[i].used && g_devs[i].hits >= 8) n++;
  return n;
}

static int countProbe() {
  int n = 0;
  for (int i = 0; i < MAX_DEV; i++) if (g_devs[i].used && (g_devs[i].kind & KIND_PROBE)) n++;
  return n;
}

static bool isAlert(const Dev& d) {
  return d.used && (d.left || (d.kind & KIND_PROBE) || d.hits >= 8 || d.tag != TAG_NONE);
}

static int countAlerts() {
  int n = 0;
  for (int i = 0; i < MAX_DEV; i++) if (isAlert(g_devs[i])) n++;
  return n;
}

static int rankOf(int i, bool alertsOnly) {
  const Dev& d = g_devs[i];
  if (!d.used) return 100000;
  if (alertsOnly && !isAlert(d)) return 100000;
  int rank = d.left ? 0 : 1000;
  if (d.kind & KIND_PROBE) rank -= 50;
  rank -= d.rssi;
  return rank;
}

static int nthSorted(int nth, bool alertsOnly) {
  bool picked[MAX_DEV] = {false};
  int chosen = -1;
  for (int step = 0; step <= nth; step++) {
    int pick = -1;
    int pickRank = 1000000;
    uint32_t pickMs = 0;
    for (int i = 0; i < MAX_DEV; i++) {
      if (picked[i]) continue;
      int r = rankOf(i, alertsOnly);
      if (r >= 100000) continue;
      if (r < pickRank || (r == pickRank && g_devs[i].lastMs > pickMs)) {
        pick = i;
        pickRank = r;
        pickMs = g_devs[i].lastMs;
      }
    }
    if (pick < 0) return -1;
    picked[pick] = true;
    chosen = pick;
  }
  return chosen;
}

static int nthUsed(int nth) { return nthSorted(nth, false); }
static int nthAlert(int nth) { return nthSorted(nth, true); }

static char kindMark(uint8_t k) {
  bool w = k & KIND_WIFI;
  bool b = k & KIND_BLE;
  bool p = k & KIND_PROBE;
  if ((w && b) || (w && p) || (b && p)) return '*';
  if (p) return 'P';
  if (b) return 'B';
  if (w) return 'W';
  return '?';
}

static void clip(const char* in, char* out, size_t n) {
  if (!in) in = "";
  size_t i = 0;
  for (; in[i] && i + 1 < n; i++) out[i] = in[i];
  out[i] = 0;
}
