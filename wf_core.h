#pragma once
// WiFighter core — device table, leftover detection, NVS, power, helpers.
// Passive observation only. Authorized use only.

#include <M5Unified.h>
#include <WiFi.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include "oui.h"
#include <esp_wifi.h>
#include <esp_sleep.h>

#define FW_VERSION "1.6.1"
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

// Globals
Dev g_devs[MAX_DEV];
ProbeHit g_probes[PROBE_SLOTS];
uint8_t g_allow[ALLOW_SLOTS][6];
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
int g_detailIdx = -1;
uint32_t g_lastScan = 0;
uint32_t g_bootMs = 0, g_lastInputMs = 0;
char g_bootErr[32] = {0};
char g_radioErr[32] = {0};
bool g_cycleErr = false;
bool g_uiForce = false;
uint8_t g_bright = 80;
bool g_dimmed = false;
uint32_t g_btnBDown = 0;
bool g_holdConsumed = false;
char g_toast[32] = {0};
uint32_t g_toastUntil = 0;
int g_confirmKind = 0;
Preferences g_prefs;
portMUX_TYPE g_probeMux = portMUX_INITIALIZER_UNLOCKED;
bool g_bleReady = false;
bool g_bleStarted = false;
uint32_t g_phaseT = 0;
uint8_t g_probeCh = 0;
uint8_t g_hopCh = 1;
uint16_t g_probeCount = 0;
bool g_llDirty = false;
uint32_t g_lastLlSave = 0;

// Forward decls used by UI/scan
void loadPrefs();
void savePrefs();
void restoreLastLeftStub();
void setBootErr(const char* s);
void setRadioErr(const char* s);
const char* activeErr();
const char* boardLabel();
int boardMah();
const char* phaseLabel();
bool isLeftStub(const Dev& d);
void upsert(const uint8_t* mac, const char* name, int8_t rssi, Kind kind, const char* ssid, uint8_t tag);
void markLeft();
int countUsed();
int countLive();
int countLeft();
int countHigh();
int countProbe();
int countAlerts();
int nthUsed(int n);
int nthAlert(int n);
void macFmt(const uint8_t* mac, char* out, size_t n);
void clip(const char* in, char* out, int max);
char kindMark(Kind k);
const char* tagName(Tag t);
const char* ouiLookup(const uint8_t* mac);
bool allowMac(const uint8_t* mac);
void removeAllow(int idx);
void clearDevs();
void toast(const char* s);
bool toastActive();
void noteInput();
void tickPower();
int secondsUntilScan();
void openList(Screen s);
void render(bool full);
void handleA();
void handleB();
void bleMacNative(const NimBLEAddress& addr, uint8_t* mac);
uint8_t bleTag(NimBLEAdvertisedDevice* d, char* nm);
void logCycle();

// ---------- implementations ----------

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

bool isLeftStub(const Dev& d) { return d.left && d.hits == 0; }

void macFmt(const uint8_t* mac, char* out, size_t n) {
  snprintf(out, n, "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
void clip(const char* in, char* out, int max) {
  if (!in || !in[0]) { out[0] = 0; return; }
  int i = 0;
  while (in[i] && i < max - 1) { out[i] = in[i]; i++; }
  out[i] = 0;
}
char kindMark(Kind k) {
  if (k == KIND_WIFI) return 'W';
  if (k == KIND_PROBE) return 'P';
  return 'B';
}
const char* tagName(Tag t) {
  if (t == TAG_TRACKER) return "TRK";
  if (t == TAG_HIGH) return "HI";
  return "";
}

const char* ouiLookup(const uint8_t* mac) {
  char prefix[9];
  snprintf(prefix, sizeof(prefix), "%02X:%02X:%02X", mac[0], mac[1], mac[2]);
  for (int i = 0; i < OUI_COUNT; i++) {
    if (strncmp(ouiTable[i].prefix, prefix, 8) == 0) return ouiTable[i].name;
  }
  return "Unknown";
}

int findMac(const uint8_t* mac) {
  for (int i = 0; i < MAX_DEV; i++) {
    if (g_devs[i].used && memcmp(g_devs[i].mac, mac, 6) == 0) return i;
  }
  return -1;
}
int freeSlot() {
  for (int i = 0; i < MAX_DEV; i++) if (!g_devs[i].used) return i;
  return -1;
}

bool isAllowed(const uint8_t* mac) {
  for (int i = 0; i < g_allowUsed; i++) {
    if (memcmp(g_allow[i], mac, 6) == 0) return true;
  }
  return false;
}

void upsert(const uint8_t* mac, const char* name, int8_t rssi, Kind kind, const char* ssid, uint8_t tag) {
  if (isAllowed(mac)) return;
  int idx = findMac(mac);
  if (idx < 0) {
    idx = freeSlot();
    if (idx < 0) return;  // table full
    memset(&g_devs[idx], 0, sizeof(Dev));
    memcpy(g_devs[idx].mac, mac, 6);
    g_devs[idx].used = true;
  }
  Dev& d = g_devs[idx];
  d.rssi = rssi;
  d.hits++;
  d.lastMs = millis();
  d.left = false;
  if (name && name[0]) strncpy(d.name, name, 17);
  if (ssid && ssid[0]) strncpy(d.ssid, ssid, 17);
  if (kind == KIND_PROBE) d.kind = KIND_PROBE;
  else if (d.kind == KIND_WIFI && kind == KIND_BLE) { /* keep */ }
  else d.kind = kind;
  if (tag > d.tag) d.tag = (Tag)tag;
  if (d.hits > 20 && d.tag < TAG_HIGH) d.tag = TAG_HIGH;
}

void markLeft() {
  uint32_t now = millis();
  uint32_t win = (uint32_t)g_leftWinSec * 1000UL;
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
    g_llDirty = true;
  }
}

int countUsed() { int n = 0; for (int i = 0; i < MAX_DEV; i++) if (g_devs[i].used) n++; return n; }
int countLive() { int n = 0; for (int i = 0; i < MAX_DEV; i++) if (g_devs[i].used && !g_devs[i].left) n++; return n; }
int countLeft() { int n = 0; for (int i = 0; i < MAX_DEV; i++) if (g_devs[i].used && g_devs[i].left) n++; return n; }
int countHigh() { int n = 0; for (int i = 0; i < MAX_DEV; i++) if (g_devs[i].used && g_devs[i].tag >= TAG_HIGH) n++; return n; }
int countProbe() { int n = 0; for (int i = 0; i < MAX_DEV; i++) if (g_devs[i].used && g_devs[i].kind == KIND_PROBE) n++; return n; }
int countAlerts() {
  int n = 0;
  for (int i = 0; i < MAX_DEV; i++) {
    if (g_devs[i].used && (g_devs[i].left || g_devs[i].tag || g_devs[i].kind == KIND_PROBE || g_devs[i].hits > 15)) n++;
  }
  return n;
}
int nthUsed(int n) {
  int c = 0;
  for (int i = 0; i < MAX_DEV; i++) if (g_devs[i].used && c++ == n) return i;
  return -1;
}
int nthAlert(int n) {
  int c = 0;
  for (int i = 0; i < MAX_DEV; i++) {
    if (g_devs[i].used && (g_devs[i].left || g_devs[i].tag || g_devs[i].kind == KIND_PROBE || g_devs[i].hits > 15)) {
      if (c++ == n) return i;
    }
  }
  return -1;
}

bool allowMac(const uint8_t* mac) {
  if (g_allowUsed >= ALLOW_SLOTS) return false;
  if (isAllowed(mac)) return true;
  memcpy(g_allow[g_allowUsed], mac, 6);
  g_allowUsed++;
  // drop from live table
  int idx = findMac(mac);
  if (idx >= 0) g_devs[idx].used = false;
  savePrefs();
  return true;
}
void removeAllow(int idx) {
  if (idx < 0 || idx >= g_allowUsed) return;
  for (int i = idx; i < g_allowUsed - 1; i++) memcpy(g_allow[i], g_allow[i + 1], 6);
  g_allowUsed--;
  savePrefs();
}
void clearDevs() {
  memset(g_devs, 0, sizeof(g_devs));
  g_lastLeft = -1;
  g_llDirty = true;
  toast("table cleared");
}

void toast(const char* s) {
  strncpy(g_toast, s, sizeof(g_toast) - 1);
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

void tickPower() {
  if (g_llDirty && millis() - g_lastLlSave > LL_SAVE_MIN_MS) {
    // rate-limited last-left persist
    if (g_lastLeft >= 0 && g_devs[g_lastLeft].used) {
      g_prefs.begin("wifighter", false);
      g_prefs.putBytes("llmac", g_devs[g_lastLeft].mac, 6);
      g_prefs.putString("llssid", g_devs[g_lastLeft].ssid);
      g_prefs.putString("llname", g_devs[g_lastLeft].name);
      g_prefs.putChar("llkind", (char)g_devs[g_lastLeft].kind);
      g_prefs.end();
    }
    g_lastLlSave = millis();
    g_llDirty = false;
  }
  if (!g_sleepOnIdle) return;
  if (millis() - g_lastInputMs > 45000 && !g_dimmed) {
    g_dimmed = true;
    M5.Display.setBrightness(8);
  }
  if (millis() - g_lastInputMs > 180000) {
    // light sleep hint (full deep sleep would need wake pin setup)
    // kept cooperative
  }
}

int secondsUntilScan() {
  if (!g_scanning || g_phase != PH_WAIT) return 0;
  uint32_t period = (uint32_t)g_scanPeriodSec * 1000UL;
  uint32_t elapsed = millis() - g_lastScan;
  if (elapsed >= period) return 0;
  return (int)((period - elapsed) / 1000UL);
}

void openList(Screen s) {
  g_screen = s;
  g_listIdx = 0;
  g_listScroll = 0;
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
  g_prefs.end();
}
void savePrefs() {
  if (!g_prefs.begin("wifighter", false)) {
    setBootErr("NVS save fail");
    return;
  }
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
  g_prefs.end();
}

void restoreLastLeftStub() {
  g_prefs.begin("wifighter", true);
  if (g_prefs.isKey("llmac")) {
    uint8_t mac[6];
    g_prefs.getBytes("llmac", mac, 6);
    int idx = freeSlot();
    if (idx >= 0) {
      memset(&g_devs[idx], 0, sizeof(Dev));
      memcpy(g_devs[idx].mac, mac, 6);
      g_devs[idx].used = true;
      g_devs[idx].left = true;
      g_devs[idx].hits = 0;
      String ss = g_prefs.getString("llssid", "");
      String nm = g_prefs.getString("llname", "");
      if (ss.length()) strncpy(g_devs[idx].ssid, ss.c_str(), 17);
      if (nm.length()) strncpy(g_devs[idx].name, nm.c_str(), 17);
      g_devs[idx].kind = (Kind)g_prefs.getChar("llkind", KIND_WIFI);
      g_lastLeft = idx;
    }
  }
  g_prefs.end();
}

void bleMacNative(const NimBLEAddress& addr, uint8_t* mac) {
  const uint8_t* n = addr.getNative();
  for (int i = 0; i < 6; i++) mac[i] = n[5 - i];  // common endian fix
}
uint8_t bleTag(NimBLEAdvertisedDevice* d, char* nm) {
  if (d->haveName()) strncpy(nm, d->getName().c_str(), 17);
  if (d->haveManufacturerData()) {
    std::string md = d->getManufacturerData();
    if (md.length() >= 2 && (uint8_t)md[0] == 0x4C && (uint8_t)md[1] == 0x00) return TAG_TRACKER;
  }
  return TAG_NONE;
}

void logCycle() {
  Serial.printf("[WF] t=%lu live=%d left=%d high=%d probe=%d total=%d\n",
                millis(), countLive(), countLeft(), countHigh(), countProbe(), countUsed());
}

// UI dispatch (implemented in wf_ui.h after this include)
void render(bool full);
void handleA();
void handleB();
