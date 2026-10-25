# WiFighter

**M5StickC Plus / Plus2 passive BLE + WiFi leftover tracker**

Watches radios that are still talking after they leave a network: Wi-Fi access points, stations sending probe requests (remembered SSIDs, not associated), and BLE advertisers including common tracker company IDs.

> **ETHICAL USE ONLY**
> Authorized security research, personal privacy monitoring, or lab testing on networks and devices you own or have explicit written permission to observe. Unauthorized tracking or disruption is illegal.
>
> This firmware is **passive / listen-only**. Wi-Fi APs are found with a passive beacon scan, probe requests are only *received* in promiscuous mode, and BLE scanning is passive (no scan requests). It does not transmit probe requests, deauth, inject, clone APs, spoof HID, capture handshakes/PMKID, run an evil portal, associate/connect to targets, or read GATT characteristics — and pull requests adding any of that will not be accepted.
>
> **You are responsible for how you use it.** Laws on intercepting radio traffic and tracking people or devices (e.g. US 18 U.S.C. § 2511 / FCC rules, EU/UK GDPR and computer-misuse laws) vary by jurisdiction. Only monitor spaces, networks, and devices you own or are authorized in writing to assess. Do not use WiFighter to follow, profile, or locate people. MAC addresses and probe SSIDs can be personal data — don't publish captures.

## Status (v1.5.2)

> **v1.5.1 restores a broken tip.** A v1.5.0 push truncated `wf_core.h` / `wf_ui.h` so Home and Menu disappeared from the source even though this checklist still said done. Flash **v1.5.1+** only. v1.5.2 rate-limits last-left NVS writes, and hold-B on Home no longer opens the Menu.

- [x] Home screen — sweep ring with AP / probe / BLE ticks, radio flags, cycle bar, last-left card, DEV / IN / LEFT / HIGH / PRB tiles, uptime, battery %
- [x] Menu — scan, devices, alerts, allowlist, settings, clear (confirm), about, home; live counts while open
- [x] Allowlist screen — view and remove saved MACs (NVS)
- [x] Non-blocking scan state machine (Wi-Fi AP → probe hop 1/6/11 → BLE)
- [x] Device list + detail (MAC, SSID, OUI, tracker tag, radio, hits, randomized-MAC flag, last-seen)
- [x] Settings + allowlist + last-left stub + scan mode saved to NVS (last-left writes rate-limited to 1/min, flushed before sleep)
- [x] Hold BtnB → Home on every screen (hold is swallowed on Home); A = select/toggle; B = menu/next
- [x] Dim wake: first A/B press while dimmed only undims (no accidental toggle)
- [x] Home shows a sticky red error banner (WiFi / probe / BLE / NVS / boot) until a clean cycle
- [x] Passive (beacon-listen) Wi-Fi AP scan — no probe requests transmitted
- [x] Menu, Devices, Alerts, Detail refresh live without full-screen flicker
- [x] Detail → B allow confirms with a toast; Settings auto-save to NVS
- [x] Device list: LEFT first, then stronger RSSI; NVS last-left stub shows "saved"
- [x] Auto-dim after 60 s idle; optional idle deep sleep (Settings); power-button hold → deep sleep. Wake: Plus2 = power button, Plus = BtnA
- [x] Fail-visible radio/NVS errors on Serial and TFT
- [ ] Hardware-verified on both boards for v1.5.2 (compile-checked only; see Known limits)
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
| **Hold BtnPWR** | Deep sleep. Wake: **Plus2** power button, **Plus** BtnA |

## Screens

- **Home** — sweep ring, counters, cycle bar, uptime, battery % (`+` = charging, `--%` = unknown), last LEFT device (NVS stub shows **saved**). A toggles scan. B opens menu. Errors replace the radio-flag row with a red banner until the next clean cycle.
- **Menu** — scan, devices, alerts, allowlist, settings, clear, about, home.
- **Devices / Alerts** — live list (refreshes every 2 s), LEFT first then strongest RSSI; footer shows position. A opens Detail, B scrolls.
- **Detail** — MAC, OUI, RSSI, SSID/name, hits, tag, last seen, randomized-MAC flag. A back, B adds the MAC to the allowlist (toast + jump to Allowlist).
- **Allowlist** — A removes the selected MAC, B scrolls (6 slots, NVS).
- **Settings** — WiFi / BLE / probe toggles, scan period, left window, idle sleep. Every A press saves to NVS. "Done" returns Home.
- **Confirm / About** — clear-table confirm (RAM only); build, board, ~mAh, authorized-use note.

## How "left" works

1. Each cycle: passive Wi-Fi AP (beacon) scan → probe-request sniff on channels 1/6/11 → BLE advert scan. Phases yield so the UI keeps painting.
2. A probe request is a station that is not associated and is still advertising a network it used to join.
3. A MAC unseen longer than the left window (default 45 s) is marked **LEFT**. The most recent LEFT MAC is persisted in NVS and restored as a stub after reboot.
4. Allowlisted MACs are dropped and not shown again.

## Logging

- **Serial** at `115200`: boot line, per-cycle counters with millis timestamp, radio errors.
- **No microSD** on StickC Plus / Plus2 — there is no SD or JSON file logger in this firmware.

Example:

```
[WF] boot v1.5.2 board=StickC Plus2 bat~200mAh wake=0
[WF] t=12345 v1.5.2 live=6 left=1 high=2 probe=3 total=9 ch=6
```

## Known limits

- **Passive only** — no active Wi-Fi/BLE attacks by design.
- **Arduino-ESP32 2.x** (PlatformIO `espressif32` with Arduino core 2.0.17; v1.4.0 compile-checked with M5Unified 0.2.26 + NimBLE-Arduino 1.4.3). Not verified on Arduino-ESP32 3.x / IDF 5.x; NimBLE-Arduino 1.4.x APIs differ from older 1.3.x (`getDevice(i)` returns by value).
- **MAX_DEV = 28**, probe slots = 12, allowlist = 6.
- **OUI table** is a small hint list, not IEEE-complete.
- **BLE MAC** endianness corrected for NimBLE native order; still subject to random/private addresses.
- **Deep sleep**: idle sleep is **off** by default (watching leftovers needs a live UI). Auto-dim is always on after 60 s. Plus wakes on BtnA via ext0 on GPIO37 — this relies on the board's external pull-up on BtnA; if your Plus wakes immediately, leave idle sleep off and report it. On Plus2 running from battery, sleep may drop the power-hold line and fully power off; the power button then cold-boots (same result for the user). Deep-sleep wake is a reboot — the device table is RAM-only, the last LEFT stub and settings come back from NVS.
- **Passive AP scan** dwells 250 ms per channel (~3.3 s for ch 1–13). APs with long beacon intervals can be missed in one cycle and show up on the next.
- **Single loop, radio stacks on core 0**: the Wi-Fi and NimBLE host tasks run on core 0; the UI and the cooperative scan state machine run in `loop()` on core 1 and never block on a scan. Lists re-sort by RSSI on refresh, so the highlighted row can shift as signals change.
- **Last-left persistence** goes to NVS at most once every 60 s. If power is cut hard (no sleep path), you can lose up to the last minute of last-left updates. Settings and the allowlist still save right away.
- **Home sweep ring** redraws with the 400 ms Home refresh, so the sweep steps rather than spinning smoothly. This is cosmetic.
- **Battery %** is PMU-estimated; mAh values are nominal, not measured drain.

## License

MIT — use responsibly. See `LICENSE`.
