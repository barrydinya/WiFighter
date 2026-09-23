# WiFighter

**M5StickC Plus / Plus2 dual BLE + WiFi persistence tracker**

Detects nearby Wi-Fi and Bluetooth Low Energy devices, scores how long they stick around, and flags devices that have **left** the area or show high persistence (possible trackers / followers).

> **ETHICAL USE ONLY**  
> Authorized security research, personal privacy monitoring, or lab testing on networks/devices you own or have explicit written permission to observe. Unauthorized tracking, monitoring, or disruption is illegal.

## Status (v0.2)

- [x] Home screen with live stats (devices / active / left / high-persist)
- [x] Full menu navigation (BtnA select, BtnB next)
- [x] Device list view (scrollable)
- [x] Alerts / Left devices view
- [x] Settings + About stubs
- [x] Offline OUI vendor lookup table (`oui.h`)
- [x] PlatformIO environments for Plus & Plus2
- [x] Real BLE scan (NimBLE) + result ingestion
- [x] Real WiFi scan (AP BSSID + SSID + RSSI)
- [ ] AirTag / Tile / SmartTag heuristics
- [ ] Persistence scoring refinements + allowlist
- [ ] Preferences save/restore of known devices
- [ ] Power management / deep-sleep between scans
- [ ] Promiscuous mode for WiFi *stations* (clients)

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
- **Menu** – Start/Stop Scan, Device List, Alerts/Left, Settings, About, Back
- **Devices** – short MAC + RSSI + type (B/W) + persist score (color coded)
- **Alerts** – devices flagged as LEFT or high-persistence
- **Settings / About** – current thresholds + version info

## How “Left” works

1. Every scan cycle (default 5 s) the device performs a WiFi AP scan and a 3-second active BLE scan.
2. Each seen MAC is upserted: hit count++, lastSeen updated, persist score recalculated.
3. If a device is not seen for > 45 s it is marked **LEFT** and appears in the Alerts screen.
4. High persistence score (≥ 0.45) is also surfaced as a possible tracker/follower.

## Next Steps

1. Expand OUI table or move to SPIFFS manufacturer DB.
2. Add AirTag / Tile / Samsung SmartTag heuristics (manufacturer data + service UUIDs).
3. Save high-value devices to Preferences / SPIFFS.
4. Optional companion Arduino board for SD logging or GPS geotag.
5. Promiscuous WiFi for station (client) MACs.

## License

MIT – use responsibly.
