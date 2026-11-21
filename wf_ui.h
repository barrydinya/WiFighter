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
  {"#", "Dump Serial", "Print device table to Serial"},
  {"X", "Clear table", "Drop the in-memory device table"},
  {"?", "About", "Build and authorized-use note"},
  {"H", "Home", "Return to the dashboard"}
};
static const int MENU_N = 9;
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
static uint32_t g_lastLive = 0;
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
  uint16_t body = (level >= 0 && level < 20 && !charging) ? COL_RED : (charging ? COL_GREEN : COL_DIM);
  M5.Display.drawRect(x, y, 12, 6, body);
  M5.Display.fillRect(x + 12, y + 2, 2, 2, body);
  int fill = (level < 0) ? 0 : (level * 10) / 100;
  if (fill > 10) fill = 10;
  if (fill > 0) M5.Display.fillRect(x + 1, y + 1, fill, 4, body);
  if (charging) {
    M5.Display.drawPixel(x + 5, y + 1, COL_GREEN);
    M5.Display.drawPixel(x + 6, y + 2, COL_GREEN);
    M5.Display.drawPixel(x + 7, y + 3, COL_GREEN);
    M5.Display.drawPixel(x + 4, y + 3, COL_GREEN);
    M5.Display.drawPixel(x + 5, y + 4, COL_GREEN);
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
  M5.Display.setCursor(10, 102);
  M5.Display.print("A scan  B menu  hold B home");
  M5.Display.setCursor(28, 114);
  M5.Display.print("authorized use only");
  delay(500);
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
    if (isLeftStub(d)) snprintf(ago, sizeof(ago), "saved");
    else ageText(d.lastMs, ago, sizeof(ago));
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

  const char* err = activeErr();
  if (err) {
    char eb[30];
    clip(err, eb, 28);
    M5.Display.fillRect(56, 18, 180, 14, COL_RED);
    M5.Display.setTextColor(COL_WHITE, COL_RED);
    M5.Display.setCursor(60, 21);
    M5.Display.printf("! %s", eb);
  } else {
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
    M5.Display.setCursor(160, 20);
    M5.Display.setTextColor(COL_WHITE, COL_BG);
    if (!g_scanning) M5.Display.print("paused");
    else if (g_phase == PH_WAIT) M5.Display.printf("next %ds", secondsUntilScan());
    else {
      M5.Display.setTextColor(COL_AMBER, COL_BG);
      M5.Display.printf("%s", phaseLabel());
    }
    {
      uint32_t up = (millis() - g_bootMs) / 1000UL;
      char upbuf[12];
      if (up < 3600) snprintf(upbuf, sizeof(upbuf), "%lum%02lu", (unsigned long)(up / 60), (unsigned long)(up % 60));
      else snprintf(upbuf, sizeof(upbuf), "%luh%02lu", (unsigned long)(up / 3600), (unsigned long)((up / 60) % 60));
      M5.Display.setTextColor(COL_DIM, COL_BG);
      int uw = M5.Display.textWidth(upbuf);
      M5.Display.setCursor(236 - uw, 20);
      M5.Display.print(upbuf);
    }
  }

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

static void drawMenu(bool full) {
  char title[24];
  snprintf(title, sizeof(title), "MENU  %d/%d", g_menuIdx + 1, MENU_N);
  int start = menuStart();
  if (!full) {
    header(title, COL_CYAN);
    for (int row = 0; row < MENU_ROWS; row++) {
      int i = start + row;
      if (i >= MENU_N) break;
      int y = 18 + row * 20;
      bool on = (i == g_menuIdx);
      char val[8];
      menuValue(i, val, sizeof(val));
      if (!val[0]) continue;
      M5.Display.fillRect(188, y + 3, 42, 12, on ? COL_PANEL : COL_BG);
      M5.Display.setTextColor(on ? COL_CYAN : COL_DIM, on ? COL_PANEL : COL_BG);
      int vw = M5.Display.textWidth(val);
      M5.Display.setCursor(228 - vw, y + 5);
      M5.Display.print(val);
    }
    return;
  }
  M5.Display.fillScreen(COL_BG);
  header(title, COL_CYAN);
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

static void drawSimple(const char* title, const char* body) {
  M5.Display.fillScreen(COL_BG);
  header(title, COL_CYAN);
  M5.Display.setTextColor(COL_WHITE, COL_BG);
  M5.Display.setCursor(8, 30);
  M5.Display.print(body);
  footer("A back", "B next");
}

static void render(bool full) {
  if (g_screen == SCR_HOME) drawHome(full || g_painted != SCR_HOME);
  else if (g_screen == SCR_MENU) drawMenu(full || g_painted != SCR_MENU);
  else if (g_screen == SCR_ABOUT) drawSimple("ABOUT", "WiFighter v" FW_VERSION "\nPassive leftover tracker\nAuthorized use only");
  else drawSimple("SCREEN", "List/detail screens\nready — extend as needed");
  g_painted = g_screen;
}

static void handleA() {
  if (g_screen == SCR_HOME) {
    g_scanning = !g_scanning;
    toast(g_scanning ? "scan on" : "scan off");
  } else if (g_screen == SCR_MENU) {
    if (g_menuIdx == 0) { g_scanning = !g_scanning; toast(g_scanning ? "scan on" : "scan off"); }
    else if (g_menuIdx == 5) { dumpSerial(); toast("dumped"); }
    else if (g_menuIdx == 6) { memset(g_devs, 0, sizeof(g_devs)); g_lastLeft = -1; toast("cleared"); }
    else if (g_menuIdx == 8) g_screen = SCR_HOME;
    else g_screen = SCR_DEVICES;
  } else {
    g_screen = SCR_MENU;
  }
  g_uiForce = true;
}

static void handleB() {
  if (g_screen == SCR_HOME) {
    g_screen = SCR_MENU;
  } else if (g_screen == SCR_MENU) {
    g_menuIdx = (g_menuIdx + 1) % MENU_N;
  } else {
    g_screen = SCR_MENU;
  }
  g_uiForce = true;
}
