#pragma once
// WiFighter core — device table, NVS prefs, leftover detection, helpers.
// Passive / listen-only. Authorized use only.

#include <M5Unified.h>
#include <WiFi.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include "oui.h"

#define FW_VERSION "2.0.0"
#define MAX_DEV 28
#define PROBE_SLOTS 12
#define ALLOW_SLOTS 6
#define WIFI_PASSIVE_MS_PER_CH 250
#define BLE_SCAN_MS 2500
#define LL_SAVE_MIN_MS 60000UL

enum Screen {
  SCR_HOME,
  SCR_MENU,
  SCR_DEVICES,
  SCR_ALERTS,
  SCR_ALLOW,
  SCR_SETTINGS,
  SCR_DETAIL,
  SCR_CONFIRM,
  SCR_ABOUT
};

enum Phase {
  PH_WAIT,
  PH_WIFI_START,
  PH_WIFI_POLL,
  PH_PROBE_ARM,
  PH_PROBE_CH,
  PH_PROBE_DONE,
  PH_BLE,
  PH_FINISH
};

enum Kind { KIND_WIFI = 0, KIND_PROBE = 1, KIND_BLE = 2 };
enum Tag { TAG_NONE = 0, TAG_TRACKER = 1, TAG_HIGH = 2 };

struct Dev {
  bool used;
  bool left;
  uint8_t mac[6];
  char ssid[18];
  char name[18];
  int8_t rssi;
  uint16_t hits;
  Kind kind;
  Tag tag;
  uint32_t lastMs;
};

struct ProbeHit {
  bool used;
  uint8_t mac[6];
  char ssid[18];
  int8_t rssi;
};

// Global definitions
Dev g_devs[MAX_DEV] = {};
ProbeHit g_probes[PROBE_SLOTS] = {};
uint8_t g_allow[ALLOW_SLOTS][6] = {};
uint8_t g_allowUsed = 0;
int g_lastLeft = -1;
bool g_scanning = true;
Phase g_phase = PH_WAIT;
char g_status[24] = "boot";
bool g_wifiOn = true, g_bleOn = true, g_probesOn = true;
uint16_t g_scanPeriodSec = 15;
uint16_t g_leftWinSec = 45;
bool g_sleepOnIdle = false;
Screen g_screen = SCR_HOME;
int g_menuIdx = 0;
int g_listIdx = 0, g_listScroll = 0;
int g_setIdx = 0;
int g_detailIdx = 0;
uint32_t g_lastScan = 0;
uint32_t g_bootMs = 0, g_lastInputMs = 0;
char g_bootErr[32] = {0};
char g_radioErr[32] = {0};
bool g_cycleErr = false;
char g_toast[32] = {0};
uint32_t g_toastUntil = 0;
uint8_t g_bright = 80;
bool g_dimmed = false;
uint32_t g_btnBDown = 0;
bool g_holdConsumed = false;
bool g_uiForce = false;
bool g_bleStarted = false;
bool g_bleReady = false;
uint32_t g_phaseT = 0;
uint8_t g_probeCh = 0;
uint8_t g_hopCh = 1;
uint16_t g_probeCount = 0;
portMUX_TYPE g_probeMux = portMUX_INITIALIZER_UNLOCKED;
Preferences g_prefs;

const char* boardLabel() {
#ifdef WIFIGHTER_BOARD_PLUS2
  return "StickC Plus2";
#else
  return "StickC Plus";
#endif
}

int boardMah() {
#ifdef WIFIGHTER_BOARD_PLUS2
  return 200;
#else
  return 120;
#endif
}

const char* phaseLabel() {
  switch (g_phase) {
    case PH_WIFI_START: case PH_WIFI_POLL: return "WiFi";
    case PH_PROBE_ARM: case PH_PROBE_CH: case PH_PROBE_DONE: return "Probe";
    case PH_BLE: return "BLE";
    default: return "idle";
  }
}

void setBootErr(const char* s) {
  strncpy(g_bootErr, s, sizeof(g_bootErr) - 1);
  g_bootErr[sizeof(g_bootErr) - 1] = 0;
}

void setRadioErr(const char* s) {
  strncpy(g_radioErr, s, sizeof(g_radioErr) - 1);
  g_radioErr[sizeof(g_radioErr) - 1] = 0;
  g_cycleErr = true;
}

const char* activeErr() {
  if (g_bootErr[0]) return g_bootErr;
  if (g_radioErr[0]) return g_radioErr;
  return nullptr;
}

void toast(const char* s) {
  strncpy(g_toast, s, sizeof(g_toast) - 1);
  g_toast[sizeof(g_toast) - 1] = 0;
  g_toastUntil = millis() + 1800;
}

bool toastActive() { return millis() < g_toastUntil; }

void noteInput() {
  g_lastInputMs = millis();
  if (g_dimmed) {
    g_dimmed = false;
    M5.Display.setBrightness(g_bright);
  }
}

void clip(const char* src, char* dst, int max) {
  int n = 0;
  while (src[n] && n < max - 1) { dst[n] = src[n]; n++; }
  dst[n] = 0;
}

void macFmt(const uint8_t* m, char* out, size_t n) {
  snprintf(out, n, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

char kindMark(Kind k) {
  if (k == KIND_WIFI) return 'W';
  if (k == KIND_PROBE) return 'P';
  return 'B';
}

bool isLeftStub(const Dev& d) { return d.left && d.hits == 0; }

bool isAllowed(const uint8_t* mac) {
  for (int i = 0; i < g_allowUsed; i++) {
    if (memcmp(g_allow[i], mac, 6) == 0) return true;
  }
  return false;
}

const char* ouiLookup(const uint8_t* mac) {
  char prefix[9];
  snprintf(prefix, sizeof(prefix), "%02X:%02X:%02X", mac[0], mac[1], mac[2]);
  for (int i = 0; i < OUI_COUNT; i++) {
    if (strcmp(ouiTable[i].prefix, prefix) == 0) return ouiTable[i].name;
  }
  return nullptr;
}

void upsert(const uint8_t* mac, const char* name, int8_t rssi, Kind kind, const char* ssid, uint8_t tag) {
  if (isAllowed(mac)) return;
  int slot = -1;
  int freeSlot = -1;
  for (int i = 0; i < MAX_DEV; i++) {
    if (g_devs[i].used && memcmp(g_devs[i].mac, mac, 6) == 0) { slot = i; break; }
    if (!g_devs[i].used && freeSlot < 0) freeSlot = i;
  }
  if (slot < 0) {
    if (freeSlot < 0) return;
    slot = freeSlot;
    memset(&g_devs[slot], 0, sizeof(Dev));
    memcpy(g_devs[slot].mac, mac, 6);
    g_devs[slot].used = true;
  }
  Dev& d = g_devs[slot];
  d.rssi = rssi;
  d.hits++;
  d.kind = kind;
  d.lastMs = millis();
  d.left = false;
  if (name && name[0]) strncpy(d.name, name, 17);
  if (ssid && ssid[0]) strncpy(d.ssid, ssid, 17);
  if (tag > d.tag) d.tag = (Tag)tag;
  if (rssi > -45 && d.hits > 8) d.tag = TAG_HIGH;
}

void saveLastLeft() {
  if (g_lastLeft < 0 || !g_devs[g_lastLeft].used) return;
  static uint32_t lastSave = 0;
  if (millis() - lastSave < LL_SAVE_MIN_MS) return;
  lastSave = millis();
  g_prefs.putBytes("llmac", g_devs[g_lastLeft].mac, 6);
  g_prefs.putString("llssid", g_devs[g_lastLeft].ssid);
  g_prefs.putString("llname", g_devs[g_lastLeft].name);
  g_prefs.putChar("llkind", (char)g_devs[g_lastLeft].kind);
  g_prefs.putBool("llvalid", true);
}

void restoreLastLeft() {
  g_lastLeft = -1;
  if (!g_prefs.getBool("llvalid", false)) return;
  uint8_t mac[6];
  if (g_prefs.getBytes("llmac", mac, 6) != 6) return;
  int slot = -1;
  for (int i = 0; i < MAX_DEV; i++) {
    if (g_devs[i].used && memcmp(g_devs[i].mac, mac, 6) == 0) { slot = i; break; }
  }
  if (slot < 0) {
    for (int i = 0; i < MAX_DEV; i++) {
      if (!g_devs[i].used) { slot = i; break; }
    }
  }
  if (slot < 0) return;
  Dev& d = g_devs[slot];
  memcpy(d.mac, mac, 6);
  d.used = true;
  d.left = true;
  d.hits = 0;
  String ssid = g_prefs.getString("llssid", "");
  strncpy(d.ssid, ssid.c_str(), 17);
  d.ssid[17] = 0;
  String name = g_prefs.getString("llname", "");
  strncpy(d.name, name.c_str(), 17);
  d.name[17] = 0;
  d.kind = (Kind)g_prefs.getChar("llkind", 0);
  d.lastMs = millis() - 10000;
  g_lastLeft = slot;
}

void markLeft() {
  uint32_t win = (uint32_t)g_leftWinSec * 1000UL;
  uint32_t now = millis();
  int newestLeft = -1;
  uint32_t newestMs = 0;
  for (int i = 0; i < MAX_DEV; i++) {
    if (!g_devs[i].used) continue;
    if (now - g_devs[i].lastMs > win) {
      g_devs[i].left = true;
      if (g_devs[i].lastMs > newestMs) {
        newestMs = g_devs[i].lastMs;
        newestLeft = i;
      }
    }
  }
  if (newestLeft >= 0 && newestLeft != g_lastLeft) {
    g_lastLeft = newestLeft;
    saveLastLeft();
  }
}

int countUsed() { int n = 0; for (int i = 0; i < MAX_DEV; i++) if (g_devs[i].used) n++; return n; }
int countLive() { int n = 0; for (int i = 0; i < MAX_DEV; i++) if (g_devs[i].used && !g_devs[i].left) n++; return n; }
int countLeft() { int n = 0; for (int i = 0; i < MAX_DEV; i++) if (g_devs[i].used && g_devs[i].left) n++; return n; }
int countHigh() { int n = 0; for (int i = 0; i < MAX_DEV; i++) if (g_devs[i].used && g_devs[i].tag == TAG_HIGH) n++; return n; }
int countProbe() { int n = 0; for (int i = 0; i < MAX_DEV; i++) if (g_devs[i].used && g_devs[i].kind == KIND_PROBE) n++; return n; }
int countAlerts() { return countLeft() + countHigh(); }

int secondsUntilScan() {
  uint32_t period = (uint32_t)g_scanPeriodSec * 1000UL;
  uint32_t elapsed = millis() - g_lastScan;
  if (elapsed >= period) return 0;
  return (int)((period - elapsed) / 1000UL);
}

void loadPrefs() {
  g_prefs.begin("wifighter", false);
  g_wifiOn = g_prefs.getBool("wifi", true);
  g_bleOn = g_prefs.getBool("ble", true);
  g_probesOn = g_prefs.getBool("prb", true);
  g_scanPeriodSec = g_prefs.getUShort("per", 15);
  g_leftWinSec = g_prefs.getUShort("win", 45);
  g_sleepOnIdle = g_prefs.getBool("sleep", false);
  g_allowUsed = g_prefs.getUChar("allowN", 0);
  if (g_allowUsed > ALLOW_SLOTS) g_allowUsed = 0;
  for (int i = 0; i < g_allowUsed; i++) {
    char key[8]; snprintf(key, sizeof(key), "a%d", i);
    g_prefs.getBytes(key, g_allow[i], 6);
  }
}

void savePrefs() {
  g_prefs.putBool("wifi", g_wifiOn);
  g_prefs.putBool("ble", g_bleOn);
  g_prefs.putBool("prb", g_probesOn);
  g_prefs.putUShort("per", g_scanPeriodSec);
  g_prefs.putUShort("win", g_leftWinSec);
  g_prefs.putBool("sleep", g_sleepOnIdle);
  g_prefs.putUChar("allowN", g_allowUsed);
  for (int i = 0; i < g_allowUsed; i++) {
    char key[8]; snprintf(key, sizeof(key), "a%d", i);
    g_prefs.putBytes(key, g_allow[i], 6);
  }
}

void restoreLastLeftStub() {
  restoreLastLeft();
}

void tickPower() {
  if (!g_dimmed && millis() - g_lastInputMs > 25000) {
    g_dimmed = true;
    M5.Display.setBrightness(20);
  }
}

void logCycle() {
  Serial.printf("[WF] cycle done used=%d left=%d probes=%u\n", countUsed(), countLeft(), (unsigned)g_probeCount);
}

void bleMacNative(const NimBLEAddress& addr, uint8_t* out) {
  const uint8_t* p = addr.getNative();
  for (int i = 0; i < 6; i++) out[i] = p[5 - i];
}

uint8_t bleTag(NimBLEAdvertisedDevice* d, char* nameOut) {
  if (d->haveName()) {
    std::string nm = d->getName();
    strncpy(nameOut, nm.c_str(), 17);
    nameOut[17] = 0;
  }
  if (d->haveManufacturerData()) {
    std::string md = d->getManufacturerData();
    if (md.length() >= 2) {
      uint16_t cid = (uint8_t)md[0] | ((uint8_t)md[1] << 8);
      if (cid == 0x004C || cid == 0x0157 || cid == 0x00E0) return TAG_TRACKER;
    }
  }
  return TAG_NONE;
}

void dumpSerial() {
  Serial.println("--- WiFighter device table ---");
  for (int i = 0; i < MAX_DEV; i++) {
    if (!g_devs[i].used) continue;
    char mac[18];
    macFmt(g_devs[i].mac, mac, sizeof(mac));
    Serial.printf("%s rssi=%d hits=%u left=%d tag=%d ssid=%s name=%s\n",
                  mac, g_devs[i].rssi, g_devs[i].hits, g_devs[i].left, (int)g_devs[i].tag,
                  g_devs[i].ssid, g_devs[i].name);
  }
  Serial.println("--- end ---");
}

bool addAllow(const uint8_t* mac) {
  if (isAllowed(mac) || g_allowUsed >= ALLOW_SLOTS) return false;
  memcpy(g_allow[g_allowUsed], mac, 6);
  g_allowUsed++;
  savePrefs();
  return true;
}

void removeAllow(int idx) {
  if (idx < 0 || idx >= g_allowUsed) return;
  for (int i = idx; i < g_allowUsed - 1; i++) {
    memcpy(g_allow[i], g_allow[i + 1], 6);
  }
  g_allowUsed--;
  savePrefs();
}
