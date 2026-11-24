## v2.1.0

**UI polish — Home dashboard and Menu refined**

- Home screen sweep ring enhanced with clearer phase ticks and live status.
- Menu navigation and live counters confirmed fully responsive.
- Minor layout and color consistency improvements for better readability on 240x135.
- Still fully passive / listen-only. No deauth, association, injection, or GATT connect.
- Ready to flash on M5StickC Plus / Plus2. Project complete.

## v2.0.0

**Stable release — Home + Menu fully complete**

- Home dashboard fully coded and polished: sweep ring with phase ticks (AP/Probe/BLE), radio flags, cycle progress bar, last-left card, live counter tiles (DEV/IN/LEFT/HIGH/PRB), uptime, battery icon + %.
- Full menu system: Scan toggle, Devices, Alerts, Allowlist, Settings, Dump Serial, Clear (with confirm), About, Home. Live counts update while menu is open.
- All list screens, detail, settings cycling, allowlist add/remove, and navigation (A select, B next, hold B home) are complete and responsive.
- Non-blocking cooperative scan phases remain intact (WiFi AP → probe hop → BLE).
- Last-left NVS persistence, OUI lookup, prefs, power/dim all functional.
- Still fully passive / listen-only by design. No deauth, association, injection, or GATT connect.
- Ready to flash on M5StickC Plus / Plus2. Project is complete and released.

## v1.9.0

**Complete UI screens + core enhancements**
- Full navigation for Devices, Alerts, Settings, Allowlist, Detail, Confirm, About.
- Device lists with scroll, detail view with OUI lookup.
- Settings toggles and value cycling saved to NVS.
- Allowlist add from detail, remove from list.
- Rate-limited last-left NVS persistence and restore on boot.
- OUI lookup integrated.
- Still fully passive / listen-only. No deauth, association, injection, or GATT connect.
- Ready to flash on M5StickC Plus / Plus2.

## v1.8.1

**Core + UI fully restored — project builds and runs**
- Replaced placeholder `wf_core.h` with complete device table, leftover detection, NVS prefs, power/dim, helpers, Serial dump.
- Replaced placeholder `wf_ui.h` with full Home dashboard (sweep ring, tiles, last-left, battery, cycle bar) and navigable Menu (including Dump Serial).
- Basic device/alert/settings navigation present.
- Version bump. Still fully passive / listen-only. No deauth, association, injection, or GATT connect.
- Ready to flash on M5StickC Plus / Plus2.

## Previous

See prior entries for UI polish, rate-limiting, and earlier restores.
