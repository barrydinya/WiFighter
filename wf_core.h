#pragma once
// WiFighter core — device table, leftover detection, NVS prefs.
// Passive observation only. Authorized networks/devices only.

#include <M5Unified.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include "oui.h"

#define FW_VERSION "1.3.1"
#define MAX_DEV 28
#define PROBE_SLOTS 12
#define ALLOW_SLOTS 6
#define SCAN_PERIOD_MS 8000
#define BLE_SCAN_MS 2000
#define HIGH_HITS 8

enum Screen : uint8_t {
  SCR_HOME = 0,
  SCR_MENU,
  SCR_DEVICES,
  SCR_ALERTS,
  SCR_ALLOW,
  SCR_DETAIL,
  SCR_SETTINGS,
  SCR_ABOUT,
  SCR_CONFIRM
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
static bool g_uiForce = false;

static bool g_scanning = true;
static bool g_wifiOn = true;
static bool g_bleOn = true;
static bool g_probesOn = true;
static uint16_t g_leftWinSec = 45;
static uint16_t g_scanPeriodSec = 8;

static Phase g_phase = PH_WAIT;
static uint32_t g_lastScan = 0;
static uint32_t g_phaseT = 0;

static bool g_bleReady = false;
static bool g_bleStarted = false;
static uint8_t g_probeCh = 0;
static uint8_t g_hopCh = 1;
static uint16_t g_probeCount = 0;

static int g_menuIdx = 0;
static int g_listIdx = 0;
static int g_listScroll = 0;
static int g_detailIdx = -1;
static int g_setIdx = 0;
static int g_confirmKind = 0;
static int g_lastLeft = -1;

static void clip(const char* in, char* out, size_t n) {
  if (!out || n == 0) return;
  if (!in) { out[0] = 0; return; }
  size_t i = 0;
  for (; i + 1 < n && in[i]; i++) out[i] = in[i];
  out[i] = 0;
}

static void macFmt(const uint8_t* mac, char* out, size_t n) {
  if (!mac || !out || n < 18) {
    if (out && n) out[0] = 0;
    return;
  }
  snprintf(out, n, "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static const char* ouiLookup(const uint8_t* mac) {
  if (!mac) return "?";
  char pref[9];
  snprintf(pref, sizeof(pref), "%02X:%02X:%02X", mac[0], mac[1], mac[2]);
  for (int i = 0; i < OUI_COUNT; i++) {
    if (strcasecmp(pref, ouiTable[i].prefix) == 0) return ouiTable[i].name;
  }
  return "unknown";
}

static char kindMark(uint8_t kind) {
  bool w = (kind & KIND_WIFI) != 0;
  bool b = (kind & KIND_BLE) != 0;
  bool p = (kind & KIND_PROBE) != 0;
  if (w && b) return '*';
  if (w) return 'W';
  if (b) return 'B';
  if (p) return 'P';
  return '?';
}

static const char* tagName(uint8_t tag) {
  switch (tag) {
    case TAG_APPLE: return "Apple";
    case TAG_SAMSUNG: return "Samsung";
    case TAG_GOOGLE: return "Google";
    case TAG_TILE: return "Tile";
    default: return "none";
  }
}

static const char* phaseLabel() {
  switch (g_phase) {
    case PH_WIFI_START:
    case PH_WIFI_POLL: return "WiFi";
    case PH_PROBE_ARM:
    case PH_PROBE_CH:
    case PH_PROBE_DONE: return "probe";
    case PH_BLE: return "BLE";
    case PH_FINISH: return "done";
    default: return "wait";
  }
}

static int secondsUntilScan() {
  if (!g_scanning) return 0;
  uint32_t period = (uint32_t)g_scanPeriodSec * 1000UL;
  uint32_t elapsed = millis() - g_lastScan;
  if (elapsed >= period) return 0;
  return (int)((period - elapsed + 999UL) / 1000UL);
}

static bool macEq(const uint8_t* a, const uint8_t* b) {
  return memcmp(a, b, 6) == 0;
}

static bool isAllowed(const uint8_t* mac) {
  for (uint8_t i = 0; i < g_allowUsed; i++) {
    if (macEq(g_allow[i], mac)) return true;
  }
  return false;
}

static bool isAlert(const Dev& d) {
  if (!d.used) return false;
  if (d.left) return true;
  if (d.tag != TAG_NONE) return true;
  if (d.hits >= HIGH_HITS) return true;
  if (d.kind & KIND_PROBE) return true;
  return false;
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
  for (int i = 0; i < MAX_DEV; i++) if (g_devs[i].used && g_devs[i].hits >= HIGH_HITS) n++;
  return n;
}

static int countProbe() {
  int n = 0;
  for (int i = 0; i < MAX_DEV; i++) if (g_devs[i].used && (g_devs[i].kind & KIND_PROBE)) n++;
  return n;
}

static int countAlerts() {
  int n = 0;
  for (int i = 0; i < MAX_DEV; i++) if (isAlert(g_devs[i])) n++;
  return n;
}

// Left rows first, then newest lastMs. O(n^2) is fine for MAX_DEV=28.
static bool listBefore(int a, int b, bool alertMode) {
  const Dev& da = g_devs[a];
  const Dev& db = g_devs[b];
  if (alertMode) {
    bool aa = isAlert(da);
    bool ab = isAlert(db);
    if (aa != ab) return aa;
  }
  if (da.left != db.left) return da.left;
  return da.lastMs > db.lastMs;
}

static int nthBy(int want, bool alertMode) {
  int idx[MAX_DEV];
  int n = 0;
  for (int i = 0; i < MAX_DEV; i++) {
    if (!g_devs[i].used) continue;
    if (alertMode && !isAlert(g_devs[i])) continue;
    idx[n++] = i;
  }
  for (int i = 1; i < n; i++) {
    int v = idx[i];
    int j = i - 1;
    while (j >= 0 && listBefore(v, idx[j], alertMode)) {
      idx[j + 1] = idx[j];
      j--;
    }
    idx[j + 1] = v;
  }
  if (want < 0 || want >= n) return -1;
  return idx[want];
}

static int nthUsed(int want) { return nthBy(want, false); }
static int nthAlert(int want) { return nthBy(want, true); }

static int findDev(const uint8_t* mac) {
  for (int i = 0; i < MAX_DEV; i++) {
    if (g_devs[i].used && macEq(g_devs[i].mac, mac)) return i;
  }
  return -1;
}

static int allocDev() {
  for (int i = 0; i < MAX_DEV; i++) {
    if (!g_devs[i].used) return i;
  }
  // Evict oldest LEFT, else oldest by lastMs.
  int victim = -1;
  for (int i = 0; i < MAX_DEV; i++) {
    if (!g_devs[i].used) continue;
    if (g_devs[i].left) {
      if (victim < 0 || g_devs[i].lastMs < g_devs[victim].lastMs) victim = i;
    }
  }
  if (victim < 0) {
    for (int i = 0; i < MAX_DEV; i++) {
      if (!g_devs[i].used) continue;
      if (victim < 0 || g_devs[i].lastMs < g_devs[victim].lastMs) victim = i;
    }
  }
  if (victim >= 0) {
    if (g_lastLeft == victim) g_lastLeft = -1;
    memset(&g_devs[victim], 0, sizeof(Dev));
  }
  return victim;
}

static void upsert(const uint8_t* mac, const char* name, int8_t rssi, uint8_t kind,
                   const char* ssid, uint8_t tag) {
  if (!mac) return;
  if (isAllowed(mac)) return;
  uint32_t now = millis();
  int i = findDev(mac);
  if (i < 0) {
    i = allocDev();
    if (i < 0) return;
    memset(&g_devs[i], 0, sizeof(Dev));
    memcpy(g_devs[i].mac, mac, 6);
    g_devs[i].firstMs = now;
    g_devs[i].used = true;
  }
  Dev& d = g_devs[i];
  d.rssi = rssi;
  d.kind |= kind;
  if (tag != TAG_NONE) d.tag = tag;
  if (name && name[0]) clip(name, d.name, sizeof(d.name));
  if (ssid && ssid[0]) clip(ssid, d.ssid, sizeof(d.ssid));
  if (d.hits < 65000) d.hits++;
  d.lastMs = now;
  d.left = false;
}

static void markLeft() {
  uint32_t now = millis();
  uint32_t win = (uint32_t)g_leftWinSec * 1000UL;
  for (int i = 0; i < MAX_DEV; i++) {
    if (!g_devs[i].used) continue;
    if (now - g_devs[i].lastMs > win) {
      if (!g_devs[i].left) {
        g_devs[i].left = true;
        g_lastLeft = i;
      }
    }
  }
}

static void clearDevs() {
  memset(g_devs, 0, sizeof(g_devs));
  g_lastLeft = -1;
  g_detailIdx = -1;
  g_listIdx = 0;
  g_listScroll = 0;
}

static void savePrefs() {
  if (!g_prefs.begin("wifighter", false)) return;
  g_prefs.putBool("wifi", g_wifiOn);
  g_prefs.putBool("ble", g_bleOn);
  g_prefs.putBool("prb", g_probesOn);
  g_prefs.putBool("scan", g_scanning);
  g_prefs.putUShort("period", g_scanPeriodSec);
  g_prefs.putUShort("left", g_leftWinSec);
  g_prefs.putUChar("allowN", g_allowUsed);
  g_prefs.putBytes("allow", g_allow, sizeof(g_allow));
  g_prefs.end();
}

static void loadPrefs() {
  if (!g_prefs.begin("wifighter", true)) return;
  g_wifiOn = g_prefs.getBool("wifi", true);
  g_bleOn = g_prefs.getBool("ble", true);
  g_probesOn = g_prefs.getBool("prb", true);
  g_scanning = g_prefs.getBool("scan", true);
  g_scanPeriodSec = g_prefs.getUShort("period", 8);
  g_leftWinSec = g_prefs.getUShort("left", 45);
  if (g_scanPeriodSec < 4) g_scanPeriodSec = 4;
  if (g_scanPeriodSec > 60) g_scanPeriodSec = 60;
  if (g_leftWinSec < 15) g_leftWinSec = 15;
  if (g_leftWinSec > 300) g_leftWinSec = 300;
  g_allowUsed = g_prefs.getUChar("allowN", 0);
  if (g_allowUsed > ALLOW_SLOTS) g_allowUsed = ALLOW_SLOTS;
  size_t got = g_prefs.getBytes("allow", g_allow, sizeof(g_allow));
  if (got < sizeof(g_allow)) memset(((uint8_t*)g_allow) + got, 0, sizeof(g_allow) - got);
  g_prefs.end();
}

static void allowMac(const uint8_t* mac) {
  if (!mac || isAllowed(mac)) return;
  if (g_allowUsed >= ALLOW_SLOTS) return;
  memcpy(g_allow[g_allowUsed], mac, 6);
  g_allowUsed++;
  // Drop from live table if present.
  int i = findDev(mac);
  if (i >= 0) {
    if (g_lastLeft == i) g_lastLeft = -1;
    if (g_detailIdx == i) g_detailIdx = -1;
    memset(&g_devs[i], 0, sizeof(Dev));
  }
  savePrefs();
}

static void removeAllow(int idx) {
  if (idx < 0 || idx >= (int)g_allowUsed) return;
  for (int i = idx; i < (int)g_allowUsed - 1; i++) {
    memcpy(g_allow[i], g_allow[i + 1], 6);
  }
  g_allowUsed--;
  memset(g_allow[g_allowUsed], 0, 6);
  savePrefs();
}

static uint8_t bleTag(const NimBLEAdvertisedDevice* d, char* nm) {
  if (!d || !nm) return TAG_NONE;
  nm[0] = 0;
  if (d->haveName()) {
    clip(d->getName().c_str(), nm, 18);
    if (strncasecmp(nm, "Tile", 4) == 0) return TAG_TILE;
  }
  if (d->haveManufacturerData()) {
    std::string md = d->getManufacturerData();
    if (md.size() >= 2) {
      uint16_t cid = (uint8_t)md[0] | ((uint16_t)(uint8_t)md[1] << 8);
      if (cid == 0x004C) return TAG_APPLE;
      if (cid == 0x0075) return TAG_SAMSUNG;
      if (cid == 0x00E0) return TAG_GOOGLE;
    }
  }
  return TAG_NONE;
}

static void bleMacNative(const NimBLEAddress& addr, uint8_t* mac) {
  // NimBLE getNative() is little-endian on ESP32; flip to wire/UI order.
  const uint8_t* n = addr.getNative();
  mac[0] = n[5];
  mac[1] = n[4];
  mac[2] = n[3];
  mac[3] = n[2];
  mac[4] = n[1];
  mac[5] = n[0];
}

static void logCycle() {
  Serial.printf("[WF] v%s live=%d left=%d high=%d probe=%d total=%d ch=%u\n",
                FW_VERSION, countLive(), countLeft(), countHigh(), countProbe(),
                countUsed(), (unsigned)g_hopCh);
}
