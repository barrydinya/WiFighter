#pragma once
// WiFighter core — device table, leftover detection, NVS, power.
// Passive listen-only. Authorized use only.

#include <M5Unified.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <string.h>
#include <stdarg.h>

#define FW_VERSION "1.5.5"
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

extern Dev g_devs[MAX_DEV];
extern ProbeHit g_probes[PROBE_SLOTS];
extern uint8_t g_allow[ALLOW_SLOTS][6];
extern uint8_t g_allowUsed;
extern int g_lastLeft;
extern bool g_scanning;
extern Phase g_phase;
extern char g_status[24];
extern bool g_wifiOn, g_bleOn, g_probesOn;
extern uint16_t g_scanPeriodSec;
extern uint16_t g_leftWinSec;
extern bool g_sleepOnIdle;
extern Screen g_screen;
extern int g_menuIdx;
extern int g_listIdx, g_listScroll;
extern int g_setIdx;
extern int g_detailIdx;
extern uint32_t g_lastScan;
extern uint32_t g_bootMs, g_lastInputMs;
extern char g_bootErr[32];
extern char g_radioErr[32];
extern bool g_cycleErr;
extern bool g_dimmed;
extern uint32_t g_btnBDown;
extern bool g_holdConsumed;
extern bool g_uiForce;
extern bool g_bleStarted, g_bleReady;
extern uint8_t g_probeCh, g_hopCh;
extern uint32_t g_phaseT;
extern uint8_t g_probeCount;
extern uint8_t g_bright;
extern portMUX_TYPE g_probeMux;
extern char g_toast[32];
extern uint32_t g_toastUntil;

// Helpers
void macFmt(const uint8_t* mac, char* out, size_t n);
void clip(const char* in, char* out, size_t n);
const char* kindMark(Kind k);
const char* tagName(Tag t);
const char* ouiLookup(const uint8_t* mac);
const char* boardLabel();
int boardMah();
const char* phaseLabel();
int secondsUntilScan();
void setBootErr(const char* s);
void setRadioErr(const char* s);
const char* activeErr();
bool toastActive();
void noteInput();
void upsert(const uint8_t* mac, const char* name, int8_t rssi, Kind kind, const char* ssid, Tag tag);
void markLeft();
int countUsed();
int countLive();
int countLeft();
int countHigh();
int countProbe();
int countAlerts();
int nthUsed(int n);
int nthAlert(int n);
bool isLeftStub(const Dev& d);
void loadPrefs();
void savePrefs();
void restoreLastLeftStub();
void tickPower();
void logCycle();
void bleMacNative(const NimBLEAddress& addr, uint8_t* mac);
uint8_t bleTag(NimBLEAdvertisedDevice* d, char* nm);

// Stubs for UI that expects them
static inline bool toastActive() { return millis() < g_toastUntil; }

// ---------- implementations ----------

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
bool g_dimmed = false;
uint32_t g_btnBDown = 0;
bool g_holdConsumed = false;
bool g_uiForce = false;
bool g_bleStarted = false, g_bleReady = false;
uint8_t g_probeCh = 0, g_hopCh = 1;
uint32_t g_phaseT = 0;
uint8_t g_probeCount = 0;
uint8_t g_bright = 80;
portMUX_TYPE g_probeMux = portMUX_INITIALIZER_UNLOCKED;
char g_toast[32] = {0};
uint32_t g_toastUntil = 0;

Preferences g_prefs;

void macFmt(const uint8_t* mac, char* out, size_t n) {
  snprintf(out, n, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void clip(const char* in, char* out, size_t n) {
  if (!in || !n) { if (n) out[0] = 0; return; }
  strncpy(out, in, n - 1);
  out[n - 1] = 0;
}

const char* kindMark(Kind k) {
  if (k == KIND_WIFI) return "W";
  if (k == KIND_PROBE) return "P";
  return "B";
}

const char* tagName(Tag t) {
  if (t == TAG_TRACKER) return "TRK";
  if (t == TAG_HIGH) return "HI";
  return "";
}

const char* ouiLookup(const uint8_t* mac) {
  // Tiny table — expand as needed
  if (mac[0] == 0x00 && mac[1] == 0x1A && mac[2] == 0x11) return "Google";
  if (mac[0] == 0xDC && mac[1] == 0xA6 && mac[2] == 0x32) return "Raspberry";
  return "unk";
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

int secondsUntilScan() {
  uint32_t period = (uint32_t)g_scanPeriodSec * 1000UL;
  uint32_t elapsed = millis() - g_lastScan;
  if (elapsed >= period) return 0;
  return (int)((period - elapsed) / 1000);
}

void setBootErr(const char* s) { strncpy(g_bootErr, s, sizeof(g_bootErr) - 1); }
void setRadioErr(const char* s) { strncpy(g_radioErr, s, sizeof(g_radioErr) - 1); g_cycleErr = true; }
const char* activeErr() { return g_radioErr[0] ? g_radioErr : (g_bootErr[0] ? g_bootErr : nullptr); }

void noteInput() {
  g_lastInputMs = millis();
  if (g_dimmed) {
    g_dimmed = false;
    M5.Display.setBrightness(g_bright);
  }
}

bool isAllow(const uint8_t* mac) {
  for (int i = 0; i < g_allowUsed; i++)
    if (memcmp(g_allow[i], mac, 6) == 0) return true;
  return false;
}

void upsert(const uint8_t* mac, const char* name, int8_t rssi, Kind kind, const char* ssid, Tag tag) {
  if (isAllow(mac)) return;
  int slot = -1;
  for (int i = 0; i < MAX_DEV; i++) {
    if (g_devs[i].used && memcmp(g_devs[i].mac, mac, 6) == 0) { slot = i; break; }
    if (!g_devs[i].used && slot < 0) slot = i;
  }
  if (slot < 0) return;
  Dev& d = g_devs[slot];
  bool wasLeft = d.left;
  memcpy(d.mac, mac, 6);
  d.used = true;
  d.left = false;
  d.rssi = rssi;
  d.hits++;
  d.kind = kind;
  d.tag = tag;
  d.lastMs = millis();
  if (name && name[0]) strncpy(d.name, name, 17);
  if (ssid && ssid[0]) strncpy(d.ssid, ssid, 17);
  if (wasLeft) g_lastLeft = slot;
}

void markLeft() {
  uint32_t win = (uint32_t)g_leftWinSec * 1000UL;
  uint32_t now = millis();
  for (int i = 0; i < MAX_DEV; i++) {
    if (!g_devs[i].used) continue;
    if (now - g_devs[i].lastMs > win && !g_devs[i].left) {
      g_devs[i].left = true;
      g_lastLeft = i;
    }
  }
}

int countUsed() { int c = 0; for (int i = 0; i < MAX_DEV; i++) if (g_devs[i].used) c++; return c; }
int countLive() { int c = 0; for (int i = 0; i < MAX_DEV; i++) if (g_devs[i].used && !g_devs[i].left) c++; return c; }
int countLeft() { int c = 0; for (int i = 0; i < MAX_DEV; i++) if (g_devs[i].used && g_devs[i].left) c++; return c; }
int countHigh() { int c = 0; for (int i = 0; i < MAX_DEV; i++) if (g_devs[i].used && g_devs[i].tag == TAG_HIGH) c++; return c; }
int countProbe() { int c = 0; for (int i = 0; i < MAX_DEV; i++) if (g_devs[i].used && g_devs[i].kind == KIND_PROBE) c++; return c; }
int countAlerts() { return countLeft() + countHigh(); }

int nthUsed(int n) {
  int c = 0;
  for (int i = 0; i < MAX_DEV; i++) if (g_devs[i].used && c++ == n) return i;
  return -1;
}
int nthAlert(int n) {
  int c = 0;
  for (int i = 0; i < MAX_DEV; i++)
    if (g_devs[i].used && (g_devs[i].left || g_devs[i].tag == TAG_HIGH) && c++ == n) return i;
  return -1;
}

bool isLeftStub(const Dev& d) { return d.left && d.hits == 0; }

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
  // Simple stub restore — expand if needed
  g_lastLeft = -1;
}

void tickPower() {
  // Dim after 60s idle
  if (!g_dimmed && millis() - g_lastInputMs > 60000) {
    g_dimmed = true;
    M5.Display.setBrightness(10);
  }
  // Optional deep sleep on idle — left as future
}

void logCycle() {
  Serial.printf("[WF] t=%lu live=%d left=%d high=%d probe=%d total=%d\n",
                millis(), countLive(), countLeft(), countHigh(), countProbe(), countUsed());
}

void bleMacNative(const NimBLEAddress& addr, uint8_t* mac) {
  const uint8_t* n = addr.getNative();
  // NimBLE often little-endian; reverse for display order if needed
  for (int i = 0; i < 6; i++) mac[i] = n[5 - i];
}

uint8_t bleTag(NimBLEAdvertisedDevice* d, char* nm) {
  if (d->haveName()) strncpy(nm, d->getName().c_str(), 17);
  // Simple tracker heuristic
  if (d->haveManufacturerData()) {
    std::string md = d->getManufacturerData();
    if (md.length() >= 2 && (uint8_t)md[0] == 0x4C && (uint8_t)md[1] == 0x00) return TAG_TRACKER; // Apple
  }
  return TAG_NONE;
}

// UI handlers expected by .ino — implemented in wf_ui.h
void handleA();
void handleB();
void render(bool force);
