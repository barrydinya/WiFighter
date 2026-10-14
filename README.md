# WiFighter

**M5StickC Plus / Plus2 passive BLE + WiFi leftover tracker**

Watches radios that are still talking after they leave a network: Wi-Fi access points, stations sending probe requests (remembered SSIDs, not associated), and BLE advertisers including common tracker company IDs.

> **ETHICAL USE ONLY**
> Authorized security research, personal privacy monitoring, or lab testing on networks and devices you own or have explicit written permission to observe. Unauthorized tracking or disruption is illegal.

This firmware is **passive**. It does not deauth, inject, clone APs, spoof HID, connect to targets, or read GATT characteristics.

## Status (v1.4.0)

- [x] Home screen — sweep ring with AP / probe / BLE ticks, radio flags, cycle bar, last-left card, DEV / IN / LEFT / HIGH / PRB tiles, uptime, battery icon
- [x] Menu — scan, devices, alerts, allowlist, settings, clear (confirm), about, home; live counts while open
- [x] Allowlist screen — view and remove saved MACs
- [x] Live home paint so the dashboard does not blank every refresh
- [x] Non-blocking scan state machine (Wi-Fi AP → probe hop 1/6/11 → BLE) so Home and Menu stay responsive
- [x] Device list + detail (MAC, SSID, OUI, tracker tag, radio, hits, randomized-MAC flag, last-seen)
- [x] Alerts view, left-first sort
- [x] Settings and allowlist saved to NVS
- [x] Hold BtnB to jump Home
- [x] BLE company-ID tags (Apple, Samsung, Google) and Tile name heuristic
- [x] 6-slot MAC allowlist
- [x] Expanded offline OUI vendor database
- [x] Duty-cycled responsive scans (UI and buttons stay live; optional deeper sleep later)

Home screen and menu live in `wf_ui.h`. Device table, leftover window, and NVS prefs live in `wf_core.h`. Scan phases live in `wf_scan.h`.

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
2. Libraries: `M5Unified`, `NimBLE-Arduino` (1.4.x)
3. Open `WiFighter.ino` and upload

## Controls

| Input | Action |
| --- | --- |
| **BtnB** | Open menu / next item / scroll |
| **BtnA** | Select / back / toggle scan on Home |
| **Hold BtnB** | Jump Home |

## Screens

- **Home** — sweep ring, counters, cycle bar, uptime, last device that went LEFT and how long ago. A toggles scan. B opens menu.
- **Menu** — scan, devices, alerts, allowlist, settings, clear, about, home. Glyph + live counts. A opens, B next.
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
[WF] boot v1.4.0
[WF] v1.4.0 live=6 left=1 high=2 probe=3 total=9 ch=6
```

## License

MIT — use responsibly.
