# WiFighter

**M5StickC Plus / Plus2 dual BLE + WiFi leftover tracker**

Detects nearby Wi-Fi access points and Bluetooth Low Energy advertisers, scores how long they stick around, and flags devices that have **left** the area or show high persistence.

> **ETHICAL USE ONLY**  
> Authorized security research, personal privacy monitoring, or lab testing on networks/devices you own or have explicit written permission to observe. Unauthorized tracking, monitoring, or disruption is illegal.

This firmware is **passive**. It does not deauth, inject, clone APs, or act as HID.

## Status (v0.4.0)

- [x] Home screen — scan ring, version, radio flags, DEV/LIVE/LEFT/HIGH tiles
- [x] Menu — scan toggle, device list, leftovers, settings, clear table, about, home
- [x] Device list + per-device detail (MAC, OUI vendor, radio, hits, persist, last-seen)
- [x] Alerts view for LEFT and high-persistence radios
- [x] Settings with NVS save (WiFi/BLE enable, scan period, left-after window)
- [x] Hold BtnB to jump Home
- [x] Serial status after each scan cycle
- [x] Offline OUI table (`oui.h`)
- [x] PlatformIO envs for Plus and Plus2
- [ ] AirTag / Tile / SmartTag protocol heuristics
- [ ] Allowlist
- [ ] Promiscuous Wi-Fi station (client) leftovers
- [ ] Deep-sleep between scans

## Hardware

| Device         | Notes                             |
|----------------|-----------------------------------|
| M5StickC Plus  | Original (AXP192)                 |
| M5StickC Plus2 | Preferred — better RF + more RAM  |

## Quick start

### PlatformIO
```bash
pio run -e m5stick-c-plus2 -t upload
```

### Arduino IDE
1. Board: `M5Stick-C-Plus` or `M5StickC Plus2`
2. Libraries: `M5Unified`, `NimBLE-Arduino`
3. Open `WiFighter.ino` → Upload

## Controls

| Input | Action |
| --- | --- |
| **BtnB** | Open menu / next item / scroll |
| **BtnA** | Select / enter / back / toggle scan on Home |
| **Hold BtnB** | Jump to Home from any screen |

## Screens

- **Home** — status ring, counters, radio flags
- **Menu** — Start/Stop, Devices, Alerts/Left, Settings, Clear, About, Home
- **Devices** — name or short MAC, B/W/*, RSSI, persist score
- **Alerts** — LEFT or high-persist only
- **Detail** — full MAC, vendor, radio, hits, last-seen
- **Settings** — toggle radios, cycle period / left window, save to NVS

## How “Left” works

1. Each cycle (default 5 s) runs a Wi-Fi AP scan and a ~3 s active BLE scan.
2. Each MAC is upserted: hits++, lastSeen now, persist score updated.
3. If a device is unseen longer than the left window (default 45 s, adjustable) it is marked **LEFT**.
4. Persist ≥ 0.45 is also raised on the Alerts screen.

## Serial

`115200` baud after every cycle:

```
[WF] v0.4.0 live=6 left=1 high=2 total=9 wifi=1 ble=1
```

## License

MIT — use responsibly.
