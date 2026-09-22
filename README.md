# WiFighter

**M5StickC Plus / Plus2 dual BLE + WiFi persistence tracker**

Detects nearby Wi-Fi and Bluetooth Low Energy devices, scores how long they stick around, and flags devices that have **left** the area or show high persistence (possible trackers / followers).

> **ETHICAL USE ONLY**  
> Authorized security research, personal privacy monitoring, or lab testing on networks/devices you own or have explicit written permission to observe. Unauthorized tracking, monitoring, or disruption is illegal.

## Status (v0.1)

- [x] Home screen with live stats (devices / active / left / high-persist)
- [x] Full menu navigation (BtnA select, BtnB next)
- [x] Device list view (scrollable)
- [x] Alerts / Left devices view
- [x] Settings + About stubs
- [x] Offline OUI vendor lookup table (`oui.h`)
- [x] PlatformIO environments for Plus & Plus2
- [ ] Real BLE scan (NimBLE) + result ingestion
- [ ] Real WiFi scan + probe/SSID capture
- [ ] AirTag / Tile / SmartTag heuristics
- [ ] Persistence scoring refinements + allowlist
- [ ] Preferences save/restore of known devices
- [ ] Power management / deep-sleep between scans

## Hardware

| Device            | Notes                                      |
|-------------------|--------------------------------------------|
| M5StickC Plus     | Original (AXP192)                          |
| M5StickC Plus2    | Preferred – better RF + more memory        |

## Quick Start

### PlatformIO (recommended)
```bash
pio run -e m5stick-c-plus2 -t upload
```

### Arduino IDE
1. Board: `M5Stick-C-Plus` or `M5StickC Plus2`
2. Libraries: `M5Unified`, `NimBLE-Arduino`
3. Open `WiFighter.ino` → Upload

### Controls
- **BtnB** → open Menu / next item / scroll
- **BtnA** → select / enter / back / toggle scan on Home

## Screens

- **Home** – status circle, live counters, quick scan toggle
- **Menu** – Start Scan, Device List, Alerts/Left, Settings, About, Back
- **Devices** – short MAC + RSSI + persist score (color coded)
- **Alerts** – devices flagged as LEFT or high-persistence
- **Settings / About** – current thresholds + version info

## Next Steps (in order)

1. Wire real WiFi scan (`WiFi.scanNetworks`) and BLE scan (`NimBLEScan`).
2. Populate `devices` vector from results, update `hitCount` / `lastSeen`.
3. Expand OUI table or move to SPIFFS manufacturer DB.
4. Improve scoring (time-window distribution, ε-connectedness).
5. Save high-value devices to Preferences / SPIFFS.
6. Optional companion Arduino board for SD logging or GPS geotag.

## License

MIT – use responsibly.
