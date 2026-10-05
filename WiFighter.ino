/*
 * WiFighter — M5StickC Plus / Plus2
 * Passive BLE + WiFi leftover tracker. Authorized use only.
 */
#include "wf_core.h"
#include "wf_scan.h"
#include "wf_ui.h"

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(1);
  M5.Display.setBrightness(80);
  M5.Display.setTextSize(1);
  g_bootMs = millis();
  loadPrefs();
  drawSplash();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(80);
  esp_wifi_set_promiscuous(false);

  NimBLEDevice::init("");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  strncpy(g_status, "boot", sizeof(g_status) - 1);
  g_uiForce = true;
  drawHome(true);
  Serial.printf("[WF] boot v%s\n", FW_VERSION);
}

void loop() {
  M5.update();
  bool dirty = false;

  if (M5.BtnB.isPressed()) {
    if (g_btnBDown == 0) g_btnBDown = millis();
    if (!g_holdConsumed && millis() - g_btnBDown > 650 && g_screen != SCR_HOME) {
      g_screen = SCR_HOME;
      g_holdConsumed = true;
      g_uiForce = true;
      dirty = true;
    }
  } else {
    if (g_btnBDown && !g_holdConsumed && M5.BtnB.wasReleased()) {
      handleB();
      g_uiForce = true;
      dirty = true;
    }
    g_btnBDown = 0;
    g_holdConsumed = false;
  }

  if (M5.BtnA.wasPressed()) {
    handleA();
    g_uiForce = true;
    dirty = true;
  }

  tickScan();

  static uint32_t lastPaint = 0;
  if (dirty || millis() - lastPaint > 400) {
    render(dirty || g_uiForce);
    g_uiForce = false;
    lastPaint = millis();
  }
  delay(15);
}
