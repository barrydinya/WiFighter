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
  char b[12];
  snprintf(b, sizeof(b), "%d%%", bat);
  M5.Display.setTextColor(bat < 20 ? COL_RED : COL_DIM, 0x0000);
  int bw = M5.Display.textWidth(b);
  M5.Display.setCursor(236 - bw, 3);
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
  delay(700);
  M5.Display.setTextSize(1);
}

static void drawRing(int cx, int cy) {
  M5.Display.fillRect(cx - 28, cy - 28, 56, 56, COL_BG);
  uint32_t t = (millis() / 90) % 16;
  static const int8_t SX[16] = {18, 16, 12, 6, 0, -6, -12, -16, -18, -16, -12, -6, 0, 6, 12, 16};
  static const int8_t SY[16] = {0, 6, 12, 16, 18, 16, 12, 6, 0, -6, -12, -16, -18, -16, -12, -6};
  uint16_t col = g_scanning ? COL_CYAN : COL_DIM;
  M5.Display.drawCircle(cx, cy, 22, col);
  M5.Display.drawCircle(cx, cy, 14, COL_PANEL);
  if (g_scanning) {
    M5.Display.drawLine(cx, cy, cx + SX[t], cy + SY[t], COL_AMBER);
    M5.Display.fillCircle(cx + SX[t], cy + SY[t], 2, COL_AMBER);
  }
  // Phase ticks: AP (top), probes (right), BLE (bottom).
  uint16_t ap = (g_phase == PH_WIFI_START || g_phase == PH_WIFI_POLL) ? COL_GREEN : COL_PANEL;
  uint16_t pr = (g_phase == PH_PROBE_ARM || g_phase == PH_PROBE_CH || g_phase == PH_PROBE_DONE) ? COL_AMBER : COL_PANEL;
  uint16_t bl = (g_phase == PH_BLE) ? COL_CYAN : COL_PANEL;
  M5.Display.fillCircle(cx, cy - 22, 2, ap);
  M5.Display.fillCircle(cx + 22, cy, 2, pr);
  M5.Display.fillCircle(cx, cy + 22, 2, bl);
  M5.Display.setTextColor(g_scanning ? COL_WHITE : COL_DIM, COL_BG);
  const char* st = g_scanning ? "ON" : "OFF";
  int tw = M5.Display.textWidth(st);
  M5.Display.setCursor(cx - tw / 2, cy - 4);
  M5.Display.print(st);
}

static void stat(int x, int w, const char* label, int value, uint16_t col) {
  M5.Display.fillRoundRect(x, 96, w, 22, 3, COL_PANEL);
  M5.Display.setTextColor(COL_DIM, COL_PANEL);
  M5.Display.setCursor(x + 3, 98);
  M5.Display.print(label);
  M5.Display.setTextColor(col, COL_PANEL);
  M5.Display.setCursor(x + 3, 108);
  M5.Display.printf("%d", value);
}

static void paintCycleBar() {
  M5.Display.fillRect(4, 88, 232, 5, COL_PANEL);
  int pct = 0;
  if (!g_scanning) pct = 0;
  else if (g_phase != PH_WAIT) pct = 100;
  else {
    uint32_t period = (uint32_t)g_scanPeriodSec * 1000UL;
    uint32_t elapsed = millis() - g_lastScan;
    if (period == 0) pct = 100;
    else if (elapsed >= period) pct = 100;
    else pct = (int)((elapsed * 100UL) / period);
  }
  int w = (228 * pct) / 100;
  if (w > 0) M5.Display.fillRect(6, 89, w, 3, g_scanning ? COL_AMBER : COL_DIM);
}

static void paintLastLeft() {
  M5.Display.fillRoundRect(56, 34, 180, 50, 3, COL_PANEL);
  M5.Display.setTextColor(COL_DIM, COL_PANEL);
  M5.Display.setCursor(62, 37);
  M5.Display.print("LAST LEFT");
  if (g_lastLeft >= 0 && g_devs[g_lastLeft].used && g_devs[g_lastLeft].left) {
    Dev& d = g_devs[g_lastLeft];
    char mac[18];
    char label[18];
    char ago[8];
    macFmt(d.mac, mac, sizeof(mac));
    clip(d.ssid[0] ? d.ssid : (d.name[0] ? d.name : mac), label, 14);
    ageText(d.lastMs, ago, sizeof(ago));
    M5.Display.setTextColor(COL_WHITE, COL_PANEL);
    M5.Display.setCursor(62, 50);
    M5.Display.printf("%c %s", kindMark(d.kind), label);
    M5.Display.setTextColor(COL_RED, COL_PANEL);
    M5.Display.setCursor(62, 64);
    M5.Display.print(mac);
    M5.Display.setTextColor(COL_AMBER, COL_PANEL);
    int aw = M5.Display.textWidth(ago);
    M5.Display.setCursor(228 - aw, 64);
    M5.Display.print(ago);
  } else {
    M5.Display.setTextColor(COL_DIM, COL_PANEL);
    M5.Display.setCursor(62, 54);
    M5.Display.print("none yet");
    M5.Display.setCursor(62, 68);
    M5.Display.printf("window %us", (unsigned)g_leftWinSec);
  }
}

static void paintHomeLive() {
  header("WIFIGHTER", COL_AMBER);
  drawRing(28, 58);

  M5.Display.fillRect(56, 18, 180, 14, COL_BG);
  M5.Display.setTextColor(g_wifiOn ? COL_GREEN : COL_DIM, COL_BG);
  M5.Display.setCursor(58, 20);
  M5.Display.print(g_wifiOn ? "WIFI" : "wifi");
  M5.Display.setTextColor(g_bleOn ? COL_CYAN : COL_DIM, COL_BG);
  M5.Display.setCursor(96, 20);
  M5.Display.print(g_bleOn ? "BLE" : "ble");
  M5.Display.setTextColor(g_probesOn ? COL_AMBER : COL_DIM, COL_BG);
  M5.Display.setCursor(128, 20);
  M5.Display.print(g_probesOn ? "PRB" : "prb");
  M5.Display.setTextColor(COL_WHITE, COL_BG);
  M5.Display.setCursor(160, 20);
  if (!g_scanning) M5.Display.print("paused");
  else if (g_phase == PH_WAIT) M5.Display.printf("next %ds", secondsUntilScan());
  else M5.Display.printf("%s", phaseLabel());

  paintLastLeft();
  paintCycleBar();
  stat(4, 44, "DEV", countUsed(), COL_WHITE);
  stat(50, 44, "IN", countLive(), COL_GREEN);
  stat(96, 46, "LEFT", countLeft(), COL_RED);
  stat(144, 44, "HIGH", countHigh(), COL_AMBER);
  stat(190, 46, "PRB", countProbe(), COL_CYAN);
}

static void drawHome(bool full) {
  if (full) {
    M5.Display.fillScreen(COL_BG);
    footer("A scan", "B menu");
  }
  paintHomeLive();
}

static void menuValue(int i, char* out, size_t n) {
  if (i == 0) snprintf(out, n, "%s", g_scanning ? "ON" : "OFF");
  else if (i == 1) snprintf(out, n, "%d", countUsed());
  else if (i == 2) snprintf(out, n, "%d", countAlerts());
  else if (i == 3) snprintf(out, n, "%u", (unsigned)g_allowUsed);
  else out[0] = 0;
}

static int menuStart() {
  int start = 0;
  if (g_menuIdx > 2) start = g_menuIdx - 2;
  if (start > MENU_N - MENU_ROWS) start = MENU_N - MENU_ROWS;
  if (start < 0) start = 0;
  return start;
}

static void drawMenu() {
  M5.Display.fillScreen(COL_BG);
  char title[24];
  snprintf(title, sizeof(title), "MENU  %d/%d", g_menuIdx + 1, MENU_N);
  header(title, COL_CYAN);
  int start = menuStart();
  for (int row = 0; row < MENU_ROWS; row++) {
    int i = start + row;
    if (i >= MENU_N) break;
    int y = 18 + row * 20;
    bool on = (i == g_menuIdx);
    M5.Display.fillRoundRect(4, y, 232, 18, 2, on ? COL_PANEL : COL_BG);
    if (on) M5.Display.fillRect(4, y, 3, 18, COL_AMBER);
    M5.Display.fillCircle(16, y + 9, 6, on ? COL_AMBER : COL_PANEL);
    M5.Display.setTextColor(on ? 0x0000 : COL_DIM, on ? COL_AMBER : COL_PANEL);
    M5.Display.setCursor(13, y + 5);
    M5.Display.print(MENU_ITEMS[i].glyph);
    M5.Display.setTextColor(on ? COL_AMBER : COL_WHITE, on ? COL_PANEL : COL_BG);
    M5.Display.setCursor(28, y + 5);
    M5.Display.print(MENU_ITEMS[i].label);
    char val[8];
    menuValue(i, val, sizeof(val));
    if (val[0]) {
      M5.Display.setTextColor(on ? COL_CYAN : COL_DIM, on ? COL_PANEL : COL_BG);
      int vw = M5.Display.textWidth(val);
      M5.Display.setCursor(228 - vw, y + 5);
      M5.Display.print(val);
    }
  }
  M5.Display.fillRect(0, 98, 240, 22, COL_BG);
  M5.Display.setTextColor(COL_DIM, COL_BG);
  M5.Display.setCursor(6, 102);
  M5.Display.print(MENU_ITEMS[g_menuIdx].hint);
  M5.Display.setCursor(6, 112);
  M5.Display.setTextColor(COL_CYAN, COL_BG);
  M5.Display.print("hold B home");
  footer("A open", "B next");
}

static void drawListRow(int row, int idx, bool alertMode) {
  int y = 18 + row * 16;
  bool on = (row == (g_listIdx - g_listScroll));
  M5.Display.fillRect(0, y, 240, 16, on ? COL_PANEL : COL_BG);
  if (idx < 0) return;
  Dev& d = g_devs[idx];
  char mac[18];
  char label[14];
  macFmt(d.mac, mac, sizeof(mac));
  clip(d.ssid[0] ? d.ssid : (d.name[0] ? d.name : mac + 9), label, 12);
  uint16_t fg = d.left ? COL_RED : COL_WHITE;
  M5.Display.setTextColor(fg, on ? COL_PANEL : COL_BG);
  M5.Display.setCursor(4, y + 4);
  M5.Display.printf("%c %-11.11s %d", kindMark(d.kind), label, d.rssi);
  if (alertMode && d.tag) {
    M5.Display.setTextColor(COL_AMBER, on ? COL_PANEL : COL_BG);
    M5.Display.print(" ");
    M5.Display.print(tagName(d.tag));
  } else if (d.left) {
    M5.Display.setTextColor(COL_RED, on ? COL_PANEL : COL_BG);
    M5.Display.print(" LEFT");
  }
}

static void drawDevices() {
  M5.Display.fillScreen(COL_BG);
  header("DEVICES", COL_GREEN);
  int n = countUsed();
  if (n == 0) {
    M5.Display.setTextColor(COL_DIM, COL_BG);
    M5.Display.setCursor(8, 50);
    M5.Display.print("no devices yet");
  } else {
    if (g_listIdx >= n) g_listIdx = n - 1;
    if (g_listIdx < g_listScroll) g_listScroll = g_listIdx;
    if (g_listIdx >= g_listScroll + 6) g_listScroll = g_listIdx - 5;
    for (int row = 0; row < 6; row++) {
      int nth = g_listScroll + row;
      if (nth >= n) break;
      drawListRow(row, nthUsed(nth), false);
    }
  }
  footer("A detail", "B scroll");
}

static void drawAlerts() {
  M5.Display.fillScreen(COL_BG);
  header("ALERTS", COL_RED);
  int n = countAlerts();
  if (n == 0) {
    M5.Display.setTextColor(COL_DIM, COL_BG);
    M5.Display.setCursor(8, 50);
    M5.Display.print("no leftovers / tags");
  } else {
    if (g_listIdx >= n) g_listIdx = n - 1;
    if (g_listIdx < g_listScroll) g_listScroll = g_listIdx;
    if (g_listIdx >= g_listScroll + 6) g_listScroll = g_listIdx - 5;
    for (int row = 0; row < 6; row++) {
      int nth = g_listScroll + row;
      if (nth >= n) break;
      drawListRow(row, nthAlert(nth), true);
    }
  }
  footer("A detail", "B scroll");
}

static void drawAllow() {
  M5.Display.fillScreen(COL_BG);
  header("ALLOWLIST", COL_CYAN);
  if (g_allowUsed == 0) {
    M5.Display.setTextColor(COL_DIM, COL_BG);
    M5.Display.setCursor(8, 40);
    M5.Display.print("empty");
    M5.Display.setCursor(8, 56);
    M5.Display.print("Allow from detail (B)");
  } else {
    if (g_listIdx >= g_allowUsed) g_listIdx = g_allowUsed - 1;
    if (g_listIdx < g_listScroll) g_listScroll = g_listIdx;
    if (g_listIdx >= g_listScroll + 6) g_listScroll = g_listIdx - 5;
    for (int row = 0; row < 6; row++) {
      int i = g_listScroll + row;
      if (i >= g_allowUsed) break;
      int y = 18 + row * 16;
      bool on = (i == g_listIdx);
      char mac[18];
      macFmt(g_allow[i], mac, sizeof(mac));
      M5.Display.fillRect(0, y, 240, 16, on ? COL_PANEL : COL_BG);
      M5.Display.setTextColor(on ? COL_AMBER : COL_WHITE, on ? COL_PANEL : COL_BG);
      M5.Display.setCursor(4, y + 4);
      M5.Display.printf("%d  %s", i + 1, mac);
    }
  }
  footer("A remove", "B scroll");
}

static void drawDetail() {
  M5.Display.fillScreen(COL_BG);
  header("DETAIL", COL_AMBER);
  if (g_detailIdx < 0 || !g_devs[g_detailIdx].used) {
    M5.Display.setTextColor(COL_DIM, COL_BG);
    M5.Display.setCursor(8, 40);
    M5.Display.print("empty");
    footer("A back", "B menu");
    return;
  }
  Dev& d = g_devs[g_detailIdx];
  char mac[18];
  macFmt(d.mac, mac, sizeof(mac));
  M5.Display.setTextColor(COL_WHITE, COL_BG);
  M5.Display.setCursor(4, 20);
  M5.Display.print(mac);
  M5.Display.setCursor(4, 34);
  M5.Display.setTextColor(COL_CYAN, COL_BG);
  M5.Display.printf("%s  rssi %d", ouiLookup(d.mac), d.rssi);
  M5.Display.setTextColor(COL_WHITE, COL_BG);
  M5.Display.setCursor(4, 48);
  M5.Display.printf("ssid %s", d.ssid[0] ? d.ssid : "-");
  M5.Display.setCursor(4, 62);
  M5.Display.printf("name %s", d.name[0] ? d.name : "-");
  M5.Display.setCursor(4, 76);
  M5.Display.printf("%c hits %u  %s", kindMark(d.kind), d.hits, d.left ? "LEFT" : "HERE");
  M5.Display.setCursor(4, 90);
  M5.Display.setTextColor(d.tag ? COL_AMBER : COL_DIM, COL_BG);
  M5.Display.printf("tag %s", d.tag ? tagName(d.tag) : "none");
  char ago[8];
  ageText(d.lastMs, ago, sizeof(ago));
  M5.Display.setTextColor(COL_DIM, COL_BG);
  M5.Display.setCursor(140, 90);
  M5.Display.printf("seen %s", ago);
  M5.Display.setCursor(4, 106);
  M5.Display.printf("%s", (d.mac[0] & 0x02) ? "randomized MAC" : "hardware MAC");
  footer("A back", "B allow");
}

static void drawSettings() {
  M5.Display.fillScreen(COL_BG);
  header("SETTINGS", COL_CYAN);
  for (int i = 0; i < SET_N; i++) {
    int y = 18 + i * 16;
    bool on = (i == g_setIdx);
    M5.Display.fillRect(4, y, 232, 15, on ? COL_PANEL : COL_BG);
    M5.Display.setTextColor(on ? COL_AMBER : COL_WHITE, on ? COL_PANEL : COL_BG);
    M5.Display.setCursor(8, y + 3);
    if (i == 0) M5.Display.printf("WiFi APs      %s", g_wifiOn ? "ON" : "OFF");
    else if (i == 1) M5.Display.printf("BLE adverts   %s", g_bleOn ? "ON" : "OFF");
    else if (i == 2) M5.Display.printf("Probe sniff   %s", g_probesOn ? "ON" : "OFF");
    else if (i == 3) M5.Display.printf("Scan period   %us", (unsigned)g_scanPeriodSec);
    else if (i == 4) M5.Display.printf("Left window   %us", (unsigned)g_leftWinSec);
    else M5.Display.print("Save + Home");
  }
  footer("A change", "B next");
}

static void drawAbout() {
  M5.Display.fillScreen(COL_BG);
  header("ABOUT", COL_AMBER);
  M5.Display.setTextColor(COL_WHITE, COL_BG);
  M5.Display.setCursor(4, 22);
  M5.Display.printf("WiFighter v%s", FW_VERSION);
  M5.Display.setTextColor(COL_DIM, COL_BG);
  M5.Display.setCursor(4, 38);
  M5.Display.print("Passive BLE + WiFi watch.");
  M5.Display.setCursor(4, 52);
  M5.Display.print("Probe req = remembered SSID.");
  M5.Display.setCursor(4, 66);
  M5.Display.print("No deauth, no connect.");
  M5.Display.setCursor(4, 80);
  M5.Display.print("Own gear / written OK only.");
  M5.Display.setCursor(4, 98);
  M5.Display.setTextColor(COL_CYAN, COL_BG);
  M5.Display.print("github.com/barrydinya/WiFighter");
  footer("A home", "B menu");
}

static void drawConfirm() {
  M5.Display.fillScreen(COL_BG);
  header("CONFIRM", COL_RED);
  M5.Display.setTextColor(COL_WHITE, COL_BG);
  M5.Display.setCursor(8, 36);
  if (g_confirmKind == 1) M5.Display.print("Clear device table?");
  else M5.Display.print("Confirm action?");
  M5.Display.setTextColor(COL_DIM, COL_BG);
  M5.Display.setCursor(8, 58);
  M5.Display.print("This only clears RAM.");
  M5.Display.setCursor(8, 74);
  M5.Display.print("Allowlist stays saved.");
  footer("A yes", "B cancel");
}

static void render(bool force) {
  bool entered = (g_painted != g_screen) || force;
  if (g_screen == SCR_HOME) {
    drawHome(entered);
    g_painted = SCR_HOME;
    return;
  }
  if (g_screen == SCR_MENU) {
    drawMenu();
    g_painted = SCR_MENU;
    return;
  }
  if (!entered) return;
  switch (g_screen) {
    case SCR_DEVICES: drawDevices(); break;
    case SCR_ALERTS: drawAlerts(); break;
    case SCR_ALLOW: drawAllow(); break;
    case SCR_DETAIL: drawDetail(); break;
    case SCR_SETTINGS: drawSettings(); break;
    case SCR_ABOUT: drawAbout(); break;
    case SCR_CONFIRM: drawConfirm(); break;
    default: drawHome(true); break;
  }
  g_painted = g_screen;
}

static void openList(Screen s) {
  g_screen = s;
  g_listIdx = 0;
  g_listScroll = 0;
}

static void handleA() {
  switch (g_screen) {
    case SCR_HOME:
      g_scanning = !g_scanning;
      strncpy(g_status, g_scanning ? "armed" : "paused", sizeof(g_status) - 1);
      savePrefs();
      break;
    case SCR_MENU:
      switch (g_menuIdx) {
        case 0:
          g_scanning = !g_scanning;
          strncpy(g_status, g_scanning ? "armed" : "paused", sizeof(g_status) - 1);
          savePrefs();
          break;
        case 1: openList(SCR_DEVICES); break;
        case 2: openList(SCR_ALERTS); break;
        case 3: openList(SCR_ALLOW); break;
        case 4: g_setIdx = 0; g_screen = SCR_SETTINGS; break;
        case 5:
          g_confirmKind = 1;
          g_screen = SCR_CONFIRM;
          break;
        case 6: g_screen = SCR_ABOUT; break;
        default: g_screen = SCR_HOME; break;
      }
      break;
    case SCR_DEVICES: {
      int n = countUsed();
      if (n == 0) { g_screen = SCR_MENU; break; }
      g_detailIdx = nthUsed(g_listIdx);
      g_screen = SCR_DETAIL;
      break;
    }
    case SCR_ALERTS: {
      int n = countAlerts();
      if (n == 0) { g_screen = SCR_MENU; break; }
      g_detailIdx = nthAlert(g_listIdx);
      g_screen = SCR_DETAIL;
      break;
    }
    case SCR_ALLOW:
      if (g_allowUsed == 0) { g_screen = SCR_MENU; break; }
      removeAllow(g_listIdx);
      if (g_listIdx >= g_allowUsed) g_listIdx = g_allowUsed > 0 ? g_allowUsed - 1 : 0;
      break;
    case SCR_DETAIL:
      g_screen = SCR_MENU;
      break;
    case SCR_SETTINGS:
      if (g_setIdx == 0) g_wifiOn = !g_wifiOn;
      else if (g_setIdx == 1) g_bleOn = !g_bleOn;
      else if (g_setIdx == 2) g_probesOn = !g_probesOn;
      else if (g_setIdx == 3) {
        g_scanPeriodSec = (uint16_t)(g_scanPeriodSec >= 30 ? 4 : g_scanPeriodSec + 2);
      } else if (g_setIdx == 4) {
        g_leftWinSec = (uint16_t)(g_leftWinSec >= 120 ? 15 : g_leftWinSec + 15);
      } else {
        savePrefs();
        g_screen = SCR_HOME;
      }
      break;
    case SCR_ABOUT:
      g_screen = SCR_HOME;
      break;
    case SCR_CONFIRM:
      if (g_confirmKind == 1) clearDevs();
      g_confirmKind = 0;
      g_screen = SCR_HOME;
      break;
  }
}

static void handleB() {
  switch (g_screen) {
    case SCR_HOME:
      g_screen = SCR_MENU;
      break;
    case SCR_MENU:
      g_menuIdx = (g_menuIdx + 1) % MENU_N;
      break;
    case SCR_DEVICES: {
      int n = countUsed();
      if (n > 0) g_listIdx = (g_listIdx + 1) % n;
      break;
    }
    case SCR_ALERTS: {
      int n = countAlerts();
      if (n > 0) g_listIdx = (g_listIdx + 1) % n;
      break;
    }
    case SCR_ALLOW:
      if (g_allowUsed > 0) g_listIdx = (g_listIdx + 1) % g_allowUsed;
      break;
    case SCR_DETAIL:
      if (g_detailIdx >= 0 && g_devs[g_detailIdx].used) allowMac(g_devs[g_detailIdx].mac);
      break;
    case SCR_SETTINGS:
      g_setIdx = (g_setIdx + 1) % SET_N;
      break;
    case SCR_ABOUT:
      g_screen = SCR_MENU;
      break;
    case SCR_CONFIRM:
      g_confirmKind = 0;
      g_screen = SCR_MENU;
      break;
  }
}
