/*
 * WiFighter — M5StickC Plus / Plus2
 * Passive BLE + WiFi leftover tracker. Authorized use only.
 *
 * Legal/ethical scope: observe only networks and devices you own or have
 * explicit written permission to monitor. No deauth, injection, handshake
 * capture, evil portal, or GATT connect is implemented or planned.
 * Wi-Fi AP discovery uses a passive (beacon-listen) scan; BLE scan is passive.
 */
#include "wf_core.h"
#include "wf_scan.h"
#include "wf_ui.h"

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(1);
  g_bright = 80;
  M5.Display.setBrightness(g_bright);
  M5.Display.setTextSize(1);
  g_bootMs = millis();
  g_lastInputMs = millis();
  g_bootErr[0] = 0;

  loadPrefs();
  drawSplash();

  if (!WiFi.mode(WIFI_STA)) {
    setBootErr("WiFi.mode STA fail");
  }
  WiFi.disconnect(true, true);
  delay(80);
  esp_err_t ep = esp_wifi_set_promiscuous(false);
  if (ep != ESP_OK) {
    Serial.printf("[WF] WARN promiscuous off err=%d\n", (int)ep);
  }

  // NimBLE-Arduino 1.4.x: init() is void (no success code). Failures surface later on scan start.
  NimBLEDevice::init("");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  restoreLastLeftStub();

  strncpy(g_status, g_bootErr[0] ? "error" : "boot", sizeof(g_status) - 1);
  g_status[sizeof(g_status) - 1] = 0;
  // First cycle ~1.5s after splash so the UI paints once first.
  g_lastScan = millis() - ((uint32_t)g_scanPeriodSec * 1000UL) + 1500UL;
  g_uiForce = true;
  drawHome(true);  // boot errors render as the sticky red Home banner
  Serial.printf("[WF] boot v%s board=%s bat~%dmAh wake=%d\n", FW_VERSION, boardLabel(), boardMah(),
                (int)esp_sleep_get_wakeup_cause());
}

void loop() {
  M5.update();
  bool dirty = false;

  // First press while dimmed only wakes the panel (no toggle / navigate).
  if (M5.BtnB.isPressed()) {
    if (g_btnBDown == 0) g_btnBDown = millis();
    if (g_dimmed) {
      noteInput();
      g_holdConsumed = true;  // swallow this press; next release should not nav
      g_uiForce = true;
      dirty = true;
    } else if (!g_holdConsumed && millis() - g_btnBDown > 650 && g_screen != SCR_HOME) {
      g_screen = SCR_HOME;
      g_holdConsumed = true;
      g_uiForce = true;
      dirty = true;
      noteInput();
    }
  } else {
    if (g_btnBDown && !g_holdConsumed && M5.BtnB.wasReleased()) {
      handleB();
      g_uiForce = true;
      dirty = true;
      noteInput();
    }
    g_btnBDown = 0;
    g_holdConsumed = false;
  }

  if (M5.BtnA.wasPressed()) {
    if (g_dimmed) {
      noteInput();
    } else {
      handleA();
      noteInput();
    }
    g_uiForce = true;
    dirty = true;
  }

  tickScan();
  tickPower();

  static uint32_t lastPaint = 0;
  if (dirty || millis() - lastPaint > 400) {
    render(dirty || g_uiForce);
    g_uiForce = false;
    lastPaint = millis();
  }
  delay(15);
}
