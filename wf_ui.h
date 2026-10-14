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
  "Save + Home"
};
static const int SET_N = 6;

static Screen g_painted = (Screen)255;

static void ageText(uint32_t ms, char* out, size_t n) {
  uint32_t s = (millis() - ms) / 1000UL;
  if (s < 60) snprintf(out, n, "%lus", (unsigned long)s);
  else if (s < 3600) snprintf(out, n, "%lum", (unsigned long)(s / 60));
  else snprintf(out, n, "%luh", (unsigned long)(s / 3600));
}

static void footer(const char* left, const char* right) {
  M5.Display.fillRect(0, 122, 240, 13, 0x0000);
  M5.Display.setTextColor(COL_DIM, 0x0000);
  M5.Display.setCursor(4, 124);
  M5.Display.print(left);
  int rw = M5.Display.textWidth(right);
  M5.Display.setCursor(236 - rw, 124);
  M5.Display.print(right);
}

static void header(const char* title, uint16_t accent) {
  M5.Display.fillRect(0, 0, 240, 16, 0x0000);
  M5.Display.fillRect(0, 15, 240, 1, accent);
  M5.Display.setTextColor(accent, 0x0000);
  M5.Display.setCursor(4, 3);
  M5.Display.print(title);
  int bat = M5.Power.getBatteryLevel();
  if (bat < 0) bat = 0;
  // Simple battery icon
  int bx = 210;
  M5.Display.drawRect(bx, 4, 18, 8, COL_DIM);
  M5.Display.fillRect(bx + 18, 6, 2, 4, COL_DIM);
  int fill = (bat * 16) / 100;
  if (fill > 0) M5.Display.fillRect(bx + 1, 5, fill, 6, bat < 20 ? COL_RED : COL_GREEN);
  char b[8];
  snprintf(b, sizeof(b), "%d", bat);
  M5.Display.setTextColor(bat < 20 ? COL_RED : COL_DIM, 0x0000);
  int bw = M5.Display.textWidth(b);
  M5.Display.setCursor(bx - 4 - bw, 3);
  M5.Display.print(b);
}

static void drawSplash() {
  M5.Display.fillScreen(COL_BG);
  M5.Display.fillRect(0, 0, 240, 4, COL_AMBER);
  M5.Display.setTextColor(COL_AMBER, COL_BG);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(28, 24);
  M5.Display.print("WIFIGHTER");
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(COL_CYAN, COL_BG);
  M5.Display.setCursor(96, 50);
  M5.Display.printf("v%s", FW_VERSION);
  M5.Display.setTextColor(COL_WHITE, COL_BG);
  M5.Display.setCursor(48, 68);
  M5.Display.print("leftover watch");
  M5.Display.setTextColor(COL_DIM, COL_BG);
  M5.Display.setCursor(18, 90);
  M5.Display.print("Home dashboard  +  menu");
  M5.Display.setCursor(22, 106);
  M5.Display.print("A scan    B menu    hold B home");
  delay(500);
  M5.Display.setTextSize(1);
}
