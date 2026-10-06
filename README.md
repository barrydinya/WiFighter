# WiFighter

**M5StickC Plus / Plus2 passive BLE + WiFi leftover tracker**

Watches radios that are still talking after they leave a network: Wi-Fi access points, stations sending probe requests (remembered SSIDs, not associated), and BLE advertisers including common tracker company IDs.

> **ETHICAL USE ONLY**
> Authorized security research, personal privacy monitoring, or lab testing on networks and devices you own or have explicit written permission to observe. Unauthorized tracking or disruption is illegal.

This firmware is **passive**. It does not deauth, inject, clone APs, spoof HID, connect to targets, or read GATT characteristics.

## Status (v1.0.0)

- [x] Home screen — pulse ring, radio flags, next-scan countdown, last-left card, DEV / IN / LEFT / HIGH / PRB tiles
- [x] Menu — scan, devices, alerts, allowlist, settings, clear (confirm), about, home
- [x] Allowlist screen — view and remove saved MACs
- [x] Live home paint so the dashboard does not blank every refresh
- [x] Non-blocking BLE advert scan so Home and Menu stay responsive
- [x] Device list + detail (MAC, SSID, OUI, tracker tag, radio, hits, randomized-MAC flag, last-seen)
- [x] Alerts view, left-first sort
- [x] Settings saved to NVS
- [x] Hold BtnB to jump Home
- [x] Passive probe-request hop on channels 1 / 6 / 11 (700 ms dwell)
- [x] BLE company-ID tags (Apple, Samsung, Google) and Tile name heuristic
- [x] 6-slot MAC allowlist
- [ ] Deep sleep between scans (UI stays awake so buttons keep working)
- [ ] Full offline OUI database

Home screen and menu live in `wf_ui.h`. Device table, leftover window, and NVS prefs live in `wf_core.h`.

## Hardware

| Device | Notes |
| --- | --- |
| M5StickC Plus | Original (AXP192) |
| M5StickC Plus2 | Preferred — better RF |

## Quick start

### PlatformIO

```bash
pio run -e m5stick-c-plus2 -t upload
```

### Arduino IDE

1. Board: `M5Stick-C-Plus` or `M5StickC Plus2`
2. Libraries: `M5Unified`, `NimBLE-Arduino`
3. Open `WiFighter.ino` and upload

## Controls

| Input | Action |
| --- | --- |
| **BtnB** | Open menu / next item / scroll |
| **BtnA** | Select / back / toggle scan on Home |
| **Hold BtnB** | Jump Home |

## Screens

- **Home** — pulse ring, counters, last device that went LEFT, seconds until the next cycle. A toggles scan. B opens menu.
- **Menu** — scan, devices, alerts, allowlist, settings, clear, about, home. Live counts sit on the right.
- **Devices** — `W` AP, `B` BLE, `P` probe-only station, `*` both radios. Left rows sort first.
- **Alerts** — LEFT, probes, high persist, or tracker tag
- **Allowlist** — saved MACs. A removes the selected row.
- **Detail** — full MAC, remembered SSID, vendor, tag, randomized-MAC note. B allowlists it.
- **Settings** — radios, probe sniff, period, left window, save
- **Confirm** — clear table asks before wiping RAM

## How "left" works

1. Each cycle scans Wi-Fi APs, sniffs probe requests on 1/6/11, then scans BLE advertisements. Phases yield so the UI keeps painting.
2. A probe request is a station that is not associated and is still advertising a network it used to join.
3. A MAC unseen longer than the left window (default 45 s) is marked **LEFT**.
4. Allowlisted MACs are dropped and not shown again.

## Serial

`115200` baud:

```
[WF] v1.0.0 live=6 left=1 high=2 probe=3 total=9 ch=6
```

## License

MIT — use responsibly.
