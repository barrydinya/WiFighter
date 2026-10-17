# WiFighter

**M5StickC Plus / Plus2 passive BLE + WiFi leftover tracker**

Watches radios that are still talking after they leave a network: Wi-Fi access points, stations sending probe requests (remembered SSIDs, not associated), and BLE advertisers including common tracker company IDs.

> **ETHICAL USE ONLY**
> Authorized security research, personal privacy monitoring, or lab testing on networks and devices you own or have explicit written permission to observe. Unauthorized tracking or disruption is illegal.
>
> This firmware is **passive**. It does not deauth, inject, clone APs, spoof HID, capture handshakes/PMKID, run an evil portal, connect to targets, or read GATT characteristics.

## Status (v1.3.3)

- [x] Home screen — sweep ring with AP / probe / BLE ticks, radio flags, cycle bar, last-left card, DEV / IN / LEFT / HIGH / PRB tiles, uptime, battery %
- [x] Menu — scan, devices, alerts, allowlist, settings, clear (confirm), about, home; live counts while open
- [x] Allowlist screen — view and remove saved MACs (NVS)
- [x] Non-blocking scan state machine (Wi-Fi AP → probe hop 1/6/11 → BLE)
- [x] Device list + detail (MAC, SSID, OUI, tracker tag, radio, hits, randomized-MAC flag, last-seen)
- [x] Settings + allowlist + last-left stub + scan mode saved to NVS
- [x] Hold BtnB → Home; A = select/toggle; B = menu/next
- [x] Dim wake: first A/B press while dimmed only undims (no accidental toggle)
- [x] Home surfaces radio fail status (WiFi/probe/BLE fail) in red
- [x] Device list: LEFT first, then stronger RSSI; NVS last-left stub shows "saved"
- [x] Auto-dim after 60 s idle; optional idle deep sleep (Settings); power-button hold → deep sleep (Plus2 `BtnPWR`)
- [x] Fail-visible radio/NVS errors on Serial (+ boot banner on TFT)
- [ ] Full offline OUI database (small built-in table only)
- [ ] SD / JSON log files — **not applicable**: StickC Plus/Plus2 have no microSD

Home / menu UI: `wf_ui.h`. Device table + NVS: `wf_core.h`. Scan phases: `wf_scan.h`.

## Board matrix

| Device | PMU / power | Battery (approx.) | Notes |
| --- | --- | --- | --- |
| **M5StickC Plus** | AXP192 | ~120 mAh | Build env `m5stick-c-plus` (`-DWIFIGHTER_BOARD_PLUS`) |
| **M5StickC Plus2** | GPIO power path (no AXP192) | ~200 mAh | Preferred RF; env `m5stick-c-plus2`; `BtnPWR` long-hold → deep sleep |

M5Unified detects the board at runtime (`M5.getBoard()`). Battery % comes from `M5.Power.getBatteryLevel()` — not a coulomb counter; treat mAh figures as labels only.

Stock PlatformIO `espressif32` only ships `m5stick-c`. This repo adds local board JSONs under `boards/` so Plus / Plus2 envs resolve.

## Quick start

### PlatformIO

```bash
pio run -e m5stick-c-plus2 -t upload
# or
pio run -e m5stick-c-plus -t upload
```

Uses partition `min_spiffs` (~1.9 MB app). Serial monitor: `115200`.

### Arduino IDE

1. Install **esp32** board package (Arduino-ESP32 **2.x** / IDF 4.4-era — see Known limits).
2. Board: `M5StickCPlus` or `M5StickCPlus2` (from M5Stack board defs) — or ESP32 Dev Module if needed.
3. Partition: **Min SPIFFS** (or Huge APP) — default 1.2 MB app is tight with NimBLE (~91% full).
4. Libraries: `M5Unified` (≥ 0.2.2), `NimBLE-Arduino` **1.4.x** (1.4.3 tested).
5. Open `WiFighter.ino` from the repo root and upload.

## Controls

| Input | Action |
| --- | --- |
| **BtnA** | Select / toggle scan on Home / change setting |
| **BtnB** | Open menu / next item / scroll |
| **Hold BtnB** | Jump Home |
| **Hold BtnPWR** (Plus2) | Deep sleep (wake with power button) |

## Screens

- **Home** — sweep ring, counters, cycle bar, uptime, battery %, last LEFT device (NVS stub shows **saved**). A toggles scan. B opens menu. Radio fails show in red.
- **Menu** — scan, devices, alerts, allowlist, settings, clear, about, home.
- **Devices / Alerts / Detail / Allowlist / Settings / Confirm / About** — as before; Settings adds **Idle sleep** (opt-in deep sleep after 5 min idle). About shows board name + ~mAh note.

## How "left" works

1. Each cycle: Wi-Fi AP scan → probe-request sniff on channels 1/6/11 → BLE advert scan. Phases yield so the UI keeps painting.
2. A probe request is a station that is not associated and is still advertising a network it used to join.
3. A MAC unseen longer than the left window (default 45 s) is marked **LEFT**. The most recent LEFT MAC is persisted in NVS and restored as a stub after reboot.
4. Allowlisted MACs are dropped and not shown again.

## Logging

- **Serial** at `115200`: boot line, per-cycle counters with millis timestamp, radio errors.
- **No microSD** on StickC Plus / Plus2 — there is no SD or JSON file logger in this firmware.

Example:

```
[WF] boot v1.3.3 board=StickC Plus2 bat~200mAh
[WF] t=12345 v1.3.3 live=6 left=1 high=2 probe=3 total=9 ch=6
```

## Known limits

- **Passive only** — no active Wi-Fi/BLE attacks by design.
- **Arduino-ESP32 2.x** (PlatformIO `espressif32` 6.x/7.x with Arduino core 2.0.17 here). Not verified on Arduino-ESP32 3.x / IDF 5.x; NimBLE-Arduino 1.4.x APIs differ from older 1.3.x (`getDevice(i)` returns by value).
- **MAX_DEV = 28**, probe slots = 12, allowlist = 6.
- **OUI table** is a small hint list, not IEEE-complete.
- **BLE MAC** endianness corrected for NimBLE native order; still subject to random/private addresses.
- **Deep sleep**: idle sleep is **off** by default (watching leftovers needs a live UI). Auto-dim is always on after 60 s. Plus2 power-hold sleep verified in API; Plus AXP path uses the same `M5.Power.deepSleep` call — confirm wake behavior on your unit.
- **Battery %** is PMU-estimated; mAh values are nominal, not measured drain.

## License

MIT — use responsibly. See `LICENSE`.
