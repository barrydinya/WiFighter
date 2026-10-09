#pragma once
// WiFighter core — device table, leftover detection, NVS prefs.
// Passive observation only. Authorized networks/devices only.

#include <M5Unified.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include "oui.h"

#define FW_VERSION "1.3.0"
#define MAX_DEV 28
#define PROBE_SLOTS 12
#define ALLOW_SLOTS 6
#define SCAN_PERIOD_MS 8000
#define BLE_SCAN_MS 2000

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

static bool g_scanning = true;
static bool g_wifiOn = true;
static bool g_bleOn = true;
static bool g_probesOn = true;
static uint16_t g_leftWinSec = 45;
static uint16_t g_scanPeriodSec = 8;

static Phase g_phase = PH_WAIT;
static uint32_t g_lastScan = 0;
static uint32_t g_phaseT = 0;