#pragma once
// WiFighter UI — home screen, menu, lists, detail, settings.
// Landscape 240x135 (rotation 1) on M5StickC Plus / Plus2.

#include "wf_core.h"

static const uint16_t COL_BG = 0x0841;
static const uint16_t COL_PANEL = 0x18C3;
static const uint16_t COL_AMBER = 0xFD20;
static const uint16_t COL_CYAN = 0x07FF;
static const uint16_t COL_DIM = 0x6B6D;
static const uint16_t COL_RED = 0xF800;
static const uint16_t COL_GREEN = 0x07E0;
static const uint16_t COL_WHITE = 0xFFFF;

static const char* MENU_ITEMS[] = {
  "Scan",
  "Devices",
  "Alerts",
  "Settings",
  "Allow last MAC",
  "Clear table",
  "About",
  "Home"
};
static const int MENU_N = 8;

static const char* SET_ITEMS[] = {
  "WiFi APs",
  "BLE adverts",
  "Probe sniff",
  "Scan period",
  "Left window",
  "Save + Home"
};
static const int SET_N = 6;

static void footer(const char* left, const char* right) {
  M5.Display.fillRect(0, 122, 240, 13, 0x0000);
  M5.Display.setTextColor(COL_DIM, 0x0000);
  M5.Display.setCursor(4, 124);
  M5.Display.print(left);
  M5.Display.setCursor(150, 124);
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
  char b[8];
  snprintf(b, sizeof(b), "%d%%", bat);
  M5.Display.setTextColor(bat < 20 ? COL_RED : COL_DIM, 0x0000);
  M5.Display.setCursor(200, 3);
  M5.Display.print(b);
}

static void drawSplash() {
  M5.Display.fillScreen(COL_BG);
  M5.Display.setTextColor(COL_AMBER, COL_BG);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(28, 40);
  M5.Display.print("WIFIGHTER");
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(COL_CYAN, COL_BG);
  M5.Display.setCursor(78, 68);
  M5.Display.printf("v%s", FW_VERSION);
  M5.Display.setTextColor(COL_DIM, COL_BG);
  M5.Display.setCursor(36, 90);
  M5.Display.print("passive leftover watch");
  delay(700);
  M5.Display.setTextSize(1);
}

static void drawRing(int cx, int cy) {
  uint32_t t = (millis() / 80) % 24;
  uint16_t col = g_scanning ? COL_CYAN : COL_DIM;
  M5.Display.drawCircle(cx, cy, 16, col);
  M5.Display.drawCircle(cx, cy, 12, COL_PANEL);
  int ang = (int)((t * 15) % 360);
  float r = ang * 0.0174533f;
  int x = cx + (int)(cosf(r) * 16);
  int y = cy + (int)(sinf(r) * 16);
  M5.Display.fillCircle(x, y, 2, g_scanning ? COL_AMBER : COL_DIM);
  M5.Display.setTextColor(COL_WHITE, COL_BG);
  M5.Display.setCursor(cx - 8, cy - 4);
  M5.Display.print(g_scanning ? "ON" : "OFF");
}

static void tile(int x, const char* label, int value, uint16_t col) {
  M5.Display.fillRoundRect(x, 96, 44, 24, 3, COL_PANEL);
  M5.Display.setTextColor(COL_DIM, COL_PANEL);
  M5.Display.setCursor(x + 4, 98);
  M5.Display.print(label);
  M5.Display.setTextColor(col, COL_PANEL);
  M5.Display.setCursor(x + 4, 108);
  M5.Display.printf("%d", value);
}

static void drawHome() {
  M5.Display.fillScreen(COL_BG);
  header("WIFIGHTER", COL_AMBER);
  drawRing(28, 58);

  M5.Display.setTextColor(COL_WHITE, COL_BG);
  M5.Display.setCursor(56, 22);
  M5.Display.print(g_status);
  M5.Display.setTextColor(g_wifiOn ? COL_GREEN : COL_DIM, COL_BG);
  M5.Display.setCursor(130, 22);
  M5.Display.print(g_wifiOn ? "WIFI" : "wifi");
  M5.Display.setTextColor(g_bleOn ? COL_CYAN : COL_DIM, COL_BG);
  M5.Display.setCursor(162, 22);
  M5.Display.print(g_bleOn ? "BLE" : "ble");
  M5.Display.setTextColor(g_probesOn ? COL_AMBER : COL_DIM, COL_BG);
  M5.Display.setCursor(188, 22);
  M5.Display.print(g_probesOn ? "PRB" : "prb");

  M5.Display.setTextColor(COL_DIM, COL_BG);
  M5.Display.setCursor(56, 40);
  M5.Display.print("last left");
  M5.Display.setTextColor(COL_WHITE, COL_BG);
  M5.Display.setCursor(56, 52);
  if (g_lastLeft >= 0 && g_devs[g_lastLeft].used) {
    Dev& d = g_devs[g_lastLeft];
    char mac[18];
    macFmt(d.mac, mac, sizeof(mac));
    const char* label = d.ssid[0] ? d.ssid : (d.name[0] ? d.name : mac);
    M5.Display.printf("%c %s", kindMark(d.kind), label);
    M5.Display.setCursor(56, 66);
    M5.Display.setTextColor(COL_RED, COL_BG);
    M5.Display.print(mac);
    if (d.tag) {
      M5.Display.setTextColor(COL_AMBER, COL_BG);
      M5.Display.print(" ");
      M5.Display.print(tagName(d.tag));
    }
  } else {
    M5.Display.setTextColor(COL_DIM, COL_BG);
    M5.Display.print("none yet");
    M5.Display.setCursor(56, 66);
    M5.Display.printf("window %us", (unsigned)g_leftWinSec);
  }

  tile(4, "DEV", countUsed(), COL_WHITE);
  tile(50, "IN", countLive(), COL_GREEN);
  tile(96, "LEFT", countLeft(), COL_RED);
  tile(142, "HIGH", countHigh(), COL_AMBER);
  tile(188, "PRB", countProbe(), COL_CYAN);
  footer("A scan", "B menu");
}

static void drawMenu() {
  M5.Display.fillScreen(COL_BG);
  header("MENU", COL_CYAN);
  int start = 0;
  if (g_menuIdx > 4) start = g_menuIdx - 4;
  for (int row = 0; row < 5; row++) {
    int i = start + row;
    if (i >= MENU_N) break;
    int y = 20 + row * 20;
    bool on = (i == g_menuIdx);
    M5.Display.fillRect(4, y, 232, 18, on ? COL_PANEL : COL_BG);
    M5.Display.setTextColor(on ? COL_AMBER : COL_WHITE, on ? COL_PANEL : COL_BG);
    M5.Display.setCursor(10, y + 4);
    if (i == 0) {
      M5.Display.printf("Scan          %s", g_scanning ? "ON" : "OFF");
    } else if (i == 1) {
      M5.Display.printf("Devices        %d", countUsed());
    } else if (i == 2) {
      M5.Display.printf("Alerts         %d", countAlerts());
    } else {
      M5.Display.print(MENU_ITEMS[i]);
    }
  }
  footer("A select", "B next");
}

static void drawListRow(int row, int idx, bool alertMode) {
  int y = 20 + row * 16;
  bool on = (row == (g_listIdx - g_listScroll));
  M5.Display.fillRect(0, y, 240, 16, on ? COL_PANEL : COL_BG);
  if (idx < 0) return;
  Dev& d = g_devs[idx];
  char mac[18];
  macFmt(d.mac, mac, sizeof(mac));
  uint16_t fg = d.left ? COL_RED : COL_WHITE;
  M5.Display.setTextColor(fg, on ? COL_PANEL : COL_BG);
  M5.Display.setCursor(4, y + 4);
  const char* label = d.ssid[0] ? d.ssid : (d.name[0] ? d.name : mac + 9);
  M5.Display.printf("%c %-10.10s %d", kindMark(d.kind), label, d.rssi);
  if (alertMode && d.tag) {
    M5.Display.setTextColor(COL_AMBER, on ? COL_PANEL : COL_BG);
    M5.Display.print(" ");
    M5.Display.print(tagName(d.tag));
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
  uint32_t ago = (millis() - d.lastMs) / 1000;
  M5.Display.setTextColor(COL_DIM, COL_BG);
  M5.Display.setCursor(120, 90);
  M5.Display.printf("seen %lus", (unsigned long)ago);
  footer("A back", "B allow");
}

static void drawSettings() {
  M5.Display.fillScreen(COL_BG);
  header("SETTINGS", COL_CYAN);
  for (int i = 0; i < SET_N; i++) {
    int y = 20 + i * 16;
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
  M5.Display.print("Probe req = left network.");
  M5.Display.setCursor(4, 66);
  M5.Display.print("No deauth, no connect.");
  M5.Display.setCursor(4, 80);
  M5.Display.print("Own gear / written OK only.");
  M5.Display.setCursor(4, 98);
  M5.Display.setTextColor(COL_CYAN, COL_BG);
  M5.Display.print("github.com/barrydinya");
  footer("A home", "B menu");
}

static void render() {
  switch (g_screen) {
    case SCR_HOME: drawHome(); break;
    case SCR_MENU: drawMenu(); break;
    case SCR_DEVICES: drawDevices(); break;
    case SCR_ALERTS: drawAlerts(); break;
    case SCR_DETAIL: drawDetail(); break;
    case SCR_SETTINGS: drawSettings(); break;
    case SCR_ABOUT: drawAbout(); break;
  }
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
        case 3: g_setIdx = 0; g_screen = SCR_SETTINGS; break;
        case 4:
          if (g_detailIdx >= 0 && g_devs[g_detailIdx].used) allowMac(g_devs[g_detailIdx].mac);
          else if (g_lastLeft >= 0 && g_devs[g_lastLeft].used) allowMac(g_devs[g_lastLeft].mac);
          break;
        case 5: clearDevs(); break;
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
    case SCR_DETAIL:
      if (g_detailIdx >= 0 && g_devs[g_detailIdx].used) allowMac(g_devs[g_detailIdx].mac);
      break;
    case SCR_SETTINGS:
      g_setIdx = (g_setIdx + 1) % SET_N;
      break;
    case SCR_ABOUT:
      g_screen = SCR_MENU;
      break;
  }
}
