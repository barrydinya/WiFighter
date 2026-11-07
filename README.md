# WiFighter

**M5StickC Plus / Plus2 passive BLE + WiFi leftover tracker**

Watches radios that are still talking after they leave a network: Wi-Fi access points, stations sending probe requests (remembered SSIDs, not associated), and BLE advertisers including common tracker company IDs.

> **ETHICAL USE ONLY**
> Authorized security research, personal privacy monitoring, or lab testing on networks and devices you own or have explicit written permission to observe. Unauthorized tracking or disruption is illegal.
>
> This firmware is **passive / listen-only**. Wi-Fi APs are found with a passive beacon scan, probe requests are only *received* in promiscuous mode, and BLE scanning is passive (no scan requests). It does not transmit probe requests, deauth, inject, clone APs, spoof HID, capture handshakes/PMKID, run an evil portal, associate/connect to targets, or read GATT characteristics — and pull requests adding any of that will not be accepted.
>
> **You are responsible for how you use it.** Laws on intercepting radio traffic and tracking people or devices (e.g. US 18 U.S.C. § 2511 / FCC rules, EU/UK GDPR and computer-misuse laws) vary by jurisdiction. Only monitor spaces, networks, and devices you own or are authorized in writing to assess. Do not use WiFighter to follow, profile, or locate people. MAC addresses and probe SSIDs can be personal data — don't publish captures.

## Status (v1.6.0)

> **v1.6.0 adds working last-left NVS persistence and full OUI lookup.** Home dashboard and Menu (already complete in `wf_ui.h`) are functional. Flash v1.6.0.

- [x] Home screen — sweep ring with AP / probe / BLE ticks, radio flags, cycle bar, last-left card, DEV / IN / LEFT / HIGH / PRB tiles, uptime, battery %
- [x] Menu — scan, devices, alerts, allowlist, settings, clear (confirm), about, home; live counts while open
- [x] Full device table, leftover detection, NVS prefs, power/dim
- [x] Non-blocking scan state machine (Wi-Fi AP → probe hop 1/6/11 → BLE)
- [x] Device list + detail
- [x] Settings + allowlist saved to NVS
- [x] Hold BtnB → Home; A = select/toggle; B = menu/next
- [x] Passive listen-only by design
- [x] Last-left NVS persistence (rate-limited) + full OUI table

Home / menu UI: `wf_ui.h`. Device table + NVS: `wf_core.h`. Scan phases: `wf_scan.h`.

## Quick start

### PlatformIO

```bash
pio run -e m5stick-c-plus2 -t upload
```

### Arduino IDE

Open `WiFighter.ino`, select M5StickC Plus or Plus2 board, install M5Unified + NimBLE-Arduino, upload.

## Controls

| Input | Action |
| --- | --- |
| **BtnA** | Select / toggle scan on Home / change setting |
| **BtnB** | Open menu / next item / scroll |
| **Hold BtnB** | Jump Home |

## Screens

- **Home** — sweep ring, counters, cycle bar, uptime, battery, last LEFT device. A toggles scan. B opens menu.
- **Menu** — full navigation to devices, alerts, settings, etc.
- **Devices / Alerts / Detail / Settings / Allowlist** — complete and live-updating.

## How "left" works

Devices not seen for the left window (default 45 s) are marked LEFT. Probe requests capture stations advertising networks they previously joined. The last LEFT device is persisted to NVS (rate-limited) and restored on next boot.

## License

MIT — authorized use only. See `LICENSE`.
