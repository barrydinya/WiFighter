#pragma once
// Passive Wi-Fi AP scan, probe-request hop, BLE advert scan.
// No association, no deauth, no injection, no GATT connect.

static void IRAM_ATTR sniffCb(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  const wifi_promiscuous_pkt_t* pp = (const wifi_promiscuous_pkt_t*)buf;
  const uint8_t* p = pp->payload;
  uint16_t len = pp->rx_ctrl.sig_len;
  if (len < 26) return;
  uint8_t subtype = (p[0] >> 4) & 0x0F;
  if (subtype != 0x04) return;
  const uint8_t* sa = p + 10;
  if ((sa[0] & 0x01) != 0) return;
  char ssid[18] = {0};
  uint16_t i = 24;
  while (i + 2 < len) {
    uint8_t id = p[i];
    uint8_t l = p[i + 1];
    if ((uint16_t)(i + 2 + l) > len) break;
    if (id == 0 && l > 0 && l < 18) {
      memcpy(ssid, p + i + 2, l);
      ssid[l] = 0;
      break;
    }
    if (id == 0) break;
    i = (uint16_t)(i + 2 + l);
  }
  portENTER_CRITICAL_ISR(&g_probeMux);
  int slot = -1;
  for (int n = 0; n < PROBE_SLOTS; n++) {
    if (g_probes[n].used && memcmp(g_probes[n].mac, sa, 6) == 0) { slot = n; break; }
    if (!g_probes[n].used && slot < 0) slot = n;
  }
  if (slot >= 0) {
    memcpy(g_probes[slot].mac, sa, 6);
    g_probes[slot].rssi = (int8_t)pp->rx_ctrl.rssi;
    if (ssid[0]) memcpy(g_probes[slot].ssid, ssid, 18);
    g_probes[slot].used = true;
  }
  portEXIT_CRITICAL_ISR(&g_probeMux);
}

static void onBleDone(NimBLEScanResults) {
  g_bleReady = true;
}

static void harvestBle() {
  NimBLEScan* scan = NimBLEDevice::getScan();
  if (!scan) return;
  NimBLEScanResults res = scan->getResults();
  int n = res.getCount();
  for (int i = 0; i < n; i++) {
    const NimBLEAdvertisedDevice* d = res.getDevice(i);
    if (!d) continue;
    NimBLEAddress addr = d->getAddress();
    uint8_t mac[6];
    memcpy(mac, addr.getNative(), 6);
    char nm[18] = {0};
    uint8_t tag = bleTag(d, nm);
    upsert(mac, nm, (int8_t)d->getRSSI(), KIND_BLE, nullptr, tag);
  }
  scan->clearResults();
}

static void tickScan() {
  if (!g_scanning) {
    if (g_phase != PH_WAIT || g_bleStarted) {
      esp_wifi_set_promiscuous(false);
      if (g_bleStarted) {
        NimBLEDevice::getScan()->stop();
        g_bleStarted = false;
        g_bleReady = false;
      }
      g_phase = PH_WAIT;
    }
    return;
  }
  uint32_t now = millis();
  const uint8_t chans[] = {1, 6, 11};
  uint32_t period = (uint32_t)g_scanPeriodSec * 1000UL;
  switch (g_phase) {
    case PH_WAIT:
      if (now - g_lastScan < period) return;
      g_lastScan = now;
      g_phase = g_wifiOn ? PH_WIFI_START : (g_probesOn ? PH_PROBE_ARM : (g_bleOn ? PH_BLE : PH_FINISH));
      break;
    case PH_WIFI_START:
      strncpy(g_status, "WiFi APs", sizeof(g_status) - 1);
      WiFi.scanDelete();
      WiFi.scanNetworks(true, true);
      g_phaseT = now;
      g_phase = PH_WIFI_POLL;
      break;
    case PH_WIFI_POLL: {
      int n = WiFi.scanComplete();
      if (n == WIFI_SCAN_RUNNING && now - g_phaseT < 4500) return;
      if (n > 0) {
        for (int i = 0; i < n; i++) {
          uint8_t mac[6];
          memcpy(mac, WiFi.BSSID(i), 6);
          String ssid = WiFi.SSID(i);
          char nm[18] = {0};
          if (ssid.length()) strncpy(nm, ssid.c_str(), 17);
          upsert(mac, nm, (int8_t)WiFi.RSSI(i), KIND_WIFI, nm, TAG_NONE);
        }
      }
      WiFi.scanDelete();
      g_phase = g_probesOn ? PH_PROBE_ARM : (g_bleOn ? PH_BLE : PH_FINISH);
      break;
    }
    case PH_PROBE_ARM:
      strncpy(g_status, "probe ch1", sizeof(g_status) - 1);
      memset(g_probes, 0, sizeof(g_probes));
      {
        wifi_promiscuous_filter_t filt = {};
        filt.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
        esp_wifi_set_promiscuous_filter(&filt);
        esp_wifi_set_promiscuous_rx_cb(sniffCb);
        esp_wifi_set_promiscuous(true);
      }
      g_probeCh = 0;
      g_hopCh = chans[0];
      esp_wifi_set_channel(chans[0], WIFI_SECOND_CHAN_NONE);
      g_phaseT = now;
      g_phase = PH_PROBE_CH;
      break;
    case PH_PROBE_CH:
      if (now - g_phaseT < 700) return;
      g_probeCh++;
      if (g_probeCh < 3) {
        g_hopCh = chans[g_probeCh];
        esp_wifi_set_channel(g_hopCh, WIFI_SECOND_CHAN_NONE);
        char st[16];
        snprintf(st, sizeof(st), "probe ch%u", g_hopCh);
        strncpy(g_status, st, sizeof(g_status) - 1);
        g_phaseT = now;
        return;
      }
      g_phase = PH_PROBE_DONE;
      break;
    case PH_PROBE_DONE:
      esp_wifi_set_promiscuous(false);
      g_probeCount = 0;
      for (int i = 0; i < PROBE_SLOTS; i++) {
        if (!g_probes[i].used) continue;
        g_probeCount++;
        upsert(g_probes[i].mac, g_probes[i].ssid, g_probes[i].rssi, KIND_PROBE, g_probes[i].ssid, TAG_NONE);
      }
      g_phase = g_bleOn ? PH_BLE : PH_FINISH;
      break;
    case PH_BLE:
      if (!g_bleStarted) {
        strncpy(g_status, "BLE scan", sizeof(g_status) - 1);
        NimBLEScan* scan = NimBLEDevice::getScan();
        if (!scan) { g_phase = PH_FINISH; break; }
        scan->setActiveScan(false);
        scan->setInterval(160);
        scan->setWindow(80);
        g_bleReady = false;
        if (!scan->start(BLE_SCAN_MS / 1000, onBleDone, false)) {
          g_phase = PH_FINISH;
          break;
        }
        g_bleStarted = true;
        g_phaseT = now;
        return;
      }
      if (!g_bleReady && now - g_phaseT < BLE_SCAN_MS + 1500) return;
      harvestBle();
      g_bleStarted = false;
      g_bleReady = false;
      g_phase = PH_FINISH;
      break;
    case PH_FINISH:
      markLeft();
      strncpy(g_status, "watching", sizeof(g_status) - 1);
      g_phase = PH_WAIT;
      g_lastScan = millis();
      break;
  }
}
