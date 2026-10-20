# WiFighter

**M5StickC Plus / Plus2 passive BLE + WiFi leftover tracker**

Watches radios that are still talking after they leave a network: Wi-Fi access points, stations sending probe requests (remembered SSIDs, not associated), and BLE advertisers including common tracker company IDs.

> **ETHICAL USE ONLY**
> Authorized security research, personal privacy monitoring, or lab testing on networks and devices you own or have explicit written permission to observe. Unauthorized tracking or disruption is illegal.
>
> This firmware is **passive / listen-only**. Wi-Fi APs are found with a passive beacon scan, probe requests are only *received* in promiscuous mode, and BLE scanning is passive (no scan requests). It does not transmit probe requests, deauth, inject, clone APs, spoof HID, capture handshakes/PMKID, run an evil portal, associate/connect to targets, or read GATT characteristics — and pull requests adding any of that will not be accepted.
>
> **You are responsible for how you use it.** Laws on intercepting radio traffic and tracking people or devices (e.g. US 18 U.S.C. § 2511 / FCC rules, EU/UK GDPR and computer-misuse laws) vary by jurisdiction. Only monitor spaces, networks, and devices you own or are authorized in writing to assess. Do not use WiFighter to follow, profile, or locate people. MAC addresses and probe SSIDs can be personal data — don't publish captures.

## Status (v1.5.0)

- [x] Home screen — sweep ring with AP / probe / BLE ticks, radio flags, cycle bar, last-left card, DEV / IN / LEFT / HIGH / PRB tiles, uptime, battery %
- [x] Menu — scan, devices, alerts, allowlist, settings, clear (confirm), about, home; live counts while open
- [x] Allowlist screen — view and remove saved MACs (NVS)
- [x] Non-blocking scan state machine (Wi-Fi AP → probe hop 1/6/11 → BLE)
- [x] Device list + detail (MAC, SSID, OUI, tracker tag, radio, hits, randomized-MAC flag, last-seen)
- [x] Settings + allowlist + last-left stub + scan mode saved to NVS
- [x] Hold BtnB → Home; A = select/toggle; B = menu/next
- [x] Dim wake: first A/B press while dimmed only undims (no accidental toggle)
- [x] Home shows a sticky red error banner (WiFi / probe / BLE / NVS / boot) until a clean cycle
- [x] Passive (beacon-listen) Wi-Fi AP scan — no probe requests transmitted
- [x] Menu, Devices, Alerts, Detail refresh live without full-screen flicker
- [x] Detail → B allow confirms with a toast; Settings auto-save to NVS
- [x] Device list: LEFT first, then stronger RSSI; NVS last-left stub shows "saved"
- [x] Auto-dim after 60 s idle; optional idle deep sleep (Settings); power-button hold → deep sleep. Wake: Plus2 = power button, Plus = BtnA
- [x] Fail-visible radio/NVS errors on Serial and TFT
- [ ] Hardware-verified on both boards for v1.5.0 (compile-checked only; see Known limits)
- [ ] Full offline OUI database (small built-in table only)
- [ ] SD / JSON log files — **not applicable**: StickC Plus/Plus2 have no microSD

Home / menu UI: `wf_ui.h`. Device table + NVS: `wf_core.h`. Scan phases: `wf_scan.h`.
