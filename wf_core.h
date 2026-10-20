#pragma once
// WiFighter core — device table, leftover detection, NVS prefs.
// Passive observation only. Authorized networks/devices only.

#include <M5Unified.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <esp_sleep.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include "oui.h"

#define FW_VERSION "1.5.0"
#define MAX_DEV 28
#define PROBE_SLOTS 12
#define ALLOW_SLOTS 6
#define SCAN_PERIOD_MS 8000
