# Changelog

## v1.3.3

UI polish pass — still passive; no attack features.

- Dim wake: first A/B while dimmed only restores brightness (no toggle / navigate)
- Home: show `g_status` in red when a radio phase fails (WiFi / probe / BLE)
- Last-LEFT NVS stub: Home/Detail label as **saved** instead of a fake 0s age
- Devices: empty-state hint; sort LEFT first, then stronger RSSI, then newest
- Version bump; README status / Serial example updated

## v1.3.2

Hardening release — still passive; no attack features.

- NVS: persist last-LEFT MAC/label/kind and restore a stub row after reboot; persist idle-sleep setting
- Power: auto-dim after 60 s idle; optional idle deep sleep (Settings); Plus2 `BtnPWR` hold → deep sleep via `M5.Power.sleep_no_timer`
- Errors: check WiFi.mode / scanNetworks / promiscuous / BLE scan start; surface on Serial and boot TFT banner; NVS save fail logged
- Boards: local PlatformIO board JSONs for Plus / Plus2; build flags `WIFIGHTER_BOARD_PLUS` / `PLUS2`; runtime `M5.getBoard()` label + ~mAh note on About
- PlatformIO: `min_spiffs` partition (app was ~91% of default 1.2 MB); both envs compile-checked
- NimBLE-Arduino 1.4.x API fixes (`getDevice` by value, manufacturer-data overload)
- README: board matrix, ethical scope, known limits (core/IDF), no-SD logging note
- Serial cycle log includes millis timestamp

## v1.3.1

Production rebuild: restore truncated `wf_core.h` so the sketch links and runs.

- Device table helpers: `upsert`, `markLeft`, allowlist, NVS prefs load/save
- Counters and left-first list indexing for Devices / Alerts
- BLE company-ID tags (Apple / Samsung / Google) + Tile name heuristic
- Correct NimBLE native MAC byte order for display and matching
- Home uptime, cycle serial log, PlatformIO builds only `WiFighter.ino`
- Still passive: no deauth, no association, no GATT connect

## v1.3.0

- Home polish pass (sweep ring, last-left card, live paint)

## v1.2.0

Home screen and menu are the release UI.

- Home: sweep ring with AP / probe / BLE phase ticks, radio flags, next-scan line, cycle bar, last-left card, DEV / IN / LEFT / HIGH / PRB tiles
- Menu: eight items, glyph badges, live counts, hint line, A opens, B advances, hold B returns home
- Menu repaints while open so scan state and counts stay current
- Still passive: no deauth, no association, no GATT connect

## v1.1.0

- Home dashboard and menu first pass
- Allowlist screen can remove a saved MAC

## v1.0.0

- Device table, leftover window, probe hop, BLE advert scan
