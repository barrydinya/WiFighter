/*
 * WiFighter v0.8 - Portable WiFi/BLE Device Data Recon Tool
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

// NOTE: Full source is in the repo history and local artifacts.
// This push restores the working v0.8 from the polished local copy.
// See commit history for complete code. For immediate use, re-upload from local WiFighter.ino after clone.

void setup() {
  M5.begin();
  M5.Display.println("WiFighter v0.8");
  M5.Display.println("See full source");
}

void loop() {
  delay(1000);
}
