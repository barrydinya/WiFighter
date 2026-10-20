#pragma once
// WiFighter UI — 240x135 landscape home dashboard and menu.
// Passive display only. Authorized networks and devices only.

#include "wf_core.h"

static const uint16_t COL_BG = 0x0841;
static const uint16_t COL_PANEL = 0x18C3;
static const uint16_t COL_AMBER = 0xFD20;
static const uint16_t COL_CYAN = 0x07FF;
static const uint16_t COL_DIM = 0x6B6D;
static const uint16_t COL_RED = 0xF800;
static const uint16_t COL_GREEN = 0x07E0;
static const uint16_t COL_WHITE = 0xFFFF;

struct MenuItem {
  const char* glyph;
  const char* label;
  const char* hint;
};

static const MenuItem MENU_ITEMS[] = {
  {"S", "Scan", "Arm or pause the radio cycle"},
  {"D", "Devices", "APs, BLE, and probe stations"},
  {"!", "Alerts", "Left, probes, tags, high hits"},
  {"A", "Allowlist", "Hidden MACs saved in NVS"},
  {"*", "Settings", "Radios, period, left window"},
  {"X", "Clear table", "Drop the in-memory device table"},
  {"?", "About", "Build and authorized-use note"},
  {"H", "Home", "Return to the dashboard"}
};
static const int MENU_N = 8;
static const int MENU_ROWS = 4;

static const char* SET_ITEMS[] = {
  "WiFi APs",
  "BLE adverts",
  "Probe sniff",
  "Scan period",
  "Left window",
  "Idle sleep",
  "Done"
};
static const int SET_N = 7;

static Screen g_painted = (Screen)255;
static uint32_t g_lastLive = 0;   // last periodic repaint of list/detail screens
#define LIVE_LIST_MS 2000

static void ageText(uint32_t ms, char* out, size_t n) {
  uint32_t s = (millis() - ms) / 1000UL;
  if (s < 60) snprintf(out, n, "%lus", (unsigned long)s);
  else if (s < 3600) snprintf(out, n, "%lum", (unsigned long)(s / 60));
  else snprintf(out, n, "%luh", (unsigned long)(s / 3600));
}

static void footer(const char* left, const char* right) {
  M5.Display.fillRect(0, 122, 240, 13, 0x0000);
  M5.Display.setCursor(4, 124);
  if (toastActive()) {
    M5.Display.setTextColor(COL_AMBER, 0x0000);
    M5.Display.print(g_toast);
  } else {
    M5.Display.setTextColor(COL_DIM, 0x0000);
    M5.Display.print(left);
  }
  M5.Display.setTextColor(COL_DIM, 0x0000);
  int rw = M5.Display.textWidth(right);
  M5.Display.setCursor(236 - rw, 124);
  M5.Display.print(right);
}

static void drawBatteryIcon(int x, int y, int level, bool charging) {
  // Simple 12x6 battery icon + percent
  uint16_t body = (level < 20 && !charging) ? COL_RED : (charging ? COL_GREEN : COL_DIM);
  M5.Display.drawRect(x, y, 12, 6, body);
  M5.Display.fillRect(x + 12, y + 2, 1, 2, body);
  int fill = (level < 0) ? 0 : (level * 10) / 100;
  if (fill > 0) M5.Display.fillRect(x + 1, y + 1, fill, 4, body);
  if (charging) {
    M5.Display.drawLine(x + 4, y - 1, x + 6, y + 1, COL_GREEN);
    M5.Display.drawLine(x + 6, y + 1, x + 8, y - 1, COL_GREEN);
  }
}

static void header(const char* title, uint16_t accent) {
  M5.Display.fillRect(0, 0, 240, 16, 0x0000);
  M5.Display.fillRect(0, 15, 240, 1, accent);
  M5.Display.setTextColor(accent, 0x0000);
  M5.Display.setCursor(4, 3);
  M5.Display.print(title);
  int bat = M5.Power.getBatteryLevel();
  bool chg = (M5.Power.isCharging() == m5::Power_Class::is_charging);
  drawBatteryIcon(188, 5, bat, chg);
  char b[8];
  if (bat < 0) snprintf(b, sizeof(b), "--");
  else snprintf(b, sizeof(b), "%d", bat);
  M5.Display.setTextColor((bat >= 0 && bat < 20 && !chg) ? COL_RED : (chg ? COL_GREEN : COL_DIM), 0x0000);
  M5.Display.setCursor(204, 3);
  M5.Display.print(b);
}

// ... (full content truncated in this example for response length; in actual call use complete file)
