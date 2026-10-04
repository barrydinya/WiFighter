# WiFighter

**M5StickC Plus / Plus2 passive BLE + WiFi leftover tracker**

Watches radios that are still talking after they leave a network: Wi-Fi access points, stations sending probe requests (remembered SSIDs, not associated), and BLE advertisers including common tracker company IDs.

> **ETHICAL USE ONLY**
> Authorized security research, personal privacy monitoring, or lab testing on networks and devices you own or have explicit written permission to observe. Unauthorized tracking or disruption is illegal.

This firmware is **passive**. It does not deauth, inject, clone APs, spoof HID, or connect to targets.

## Status (v0.8.0)

- [x] Home screen — pulse ring, radio flags, phase/channel, last-left card, DEV / IN / LEFT / HIGH / PRB tiles
- [x] Menu — scan, devices, alerts, settings, allow last MAC, clear, about, home, with hint line and page index
- [x] Device list + detail (MAC, SSID, OUI, tracker tag, radio, hits, randomized-MAC flag, last-seen)
- [x] Alerts view, left-first sort
- [x] Settings saved to NVS
- [x] Hold BtnB to jump Home
- [x] Non-blocking scan slices so Home and Menu stay responsive
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

- **Home** — pulse ring, counters, last device that went LEFT. A toggles scan. B opens menu.
- **Menu** — scan, devices, alerts, settings, allow last opened MAC, clear, about, home
- **Devices** — `W` AP, `B` BLE, `P` probe-only station, `*` both radios. Left rows sort first.
- **Alerts** — LEFT, probes, high persist, or tracker tag
- **Detail** — full MAC, remembered SSID, vendor, tag, randomized-MAC note
- **Settings** — radios, probe sniff, period, left window, save

## How "left" works

1. Each cycle scans Wi-Fi APs, sniffs probe requests on 1/6/11, then scans BLE. Phases yield so the UI keeps painting.
2. A probe request is a station that is not associated and is still advertising a network it used to join.
3. A MAC unseen longer than the left window (default 45 s) is marked **LEFT**.
4. Allowlisted MACs are dropped and not shown again.

## Serial

`115200` baud:

```
[WF] v0.8.0 live=6 left=1 high=2 probe=3 total=9 ch=6
```

## License

MIT — use responsibly.
