# Changelog

## v1.5.1

**Critical restore:** tip of `main` at v1.5.0 shipped *truncated* `wf_core.h` (476 B) and `wf_ui.h` (3.5 KB ending in a placeholder comment). Home, Menu, device table, NVS prefs, and power management were wiped — the sketch could not build. This release restores the full sources from the last good v1.4.0 firmware commit and keeps the intended battery-icon polish.

- Restored full `wf_core.h` (device table, leftover detection, NVS prefs, power/sleep, allowlist) and full `wf_ui.h` (Home dashboard + Menu + all screens).
- Header draws a compact battery icon + numeric level (red when low, green when charging; `--` when PMU read fails).
- Confirmed Home: sweep ring, WIFI/BLE/PRB flags, sticky red error banner, cycle bar, last-left card, DEV / IN / LEFT / HIGH / PRB tiles, uptime.
- Confirmed Menu: Scan / Devices / Alerts / Allowlist / Settings / Clear / About / Home with live counts; A open, B next, hold-B Home; non-blocking refresh.
- Still fully passive / listen-only. No deauth, association, injection, or GATT connect.

## v1.5.0

**Broken on GitHub tip** — do not flash. Claimed battery-icon polish, but the push truncated `wf_core.h` / `wf_ui.h`. Superseded by v1.5.1.

## v1.4.0

Release polish: Home/Menu were already complete (since v1.2.0); this pass fixes rough edges found in review. Still passive; no attack features.

- **Radio is now listen-only end to end**: Wi-Fi AP discovery uses a *passive* beacon scan (`WiFi.scanNetworks(async, hidden, passive=true, 250 ms/ch)`) — the Stick no longer transmits probe requests. BLE scan stays passive.
- **Sticky red error banner on Home**: WiFi / probe / channel / BLE failures (and boot/NVS errors) now stay on Home until a clean cycle. In v1.3.3 the red status was overwritten by the next phase within milliseconds, and the boot banner was painted over after 400 ms.
- New "WiFi slow" / "BLE slow" errors when a scan overruns its timeout; a stalled BLE scan is stopped before results are read.
- **StickC Plus deep-sleep wake**: M5Unified has no wake pin for AXP192 boards, so idle sleep on the Plus could only be ended by reset. Plus now arms BtnA (GPIO37, ext0) before sleeping; Plus2 keeps the power button (GPIO35). Radios are stopped first, and sleep is skipped while a button is held. Boot log prints the wake cause.
- **Menu**: periodic refresh updates only battery + live counts (no full-screen flicker every 400 ms).
- **Devices / Alerts / Detail** refresh live every 2 s in place; list footer shows position (`B 3/12`).
- **Detail → B allow**: confirms with a "MAC allowed" toast and jumps to the Allowlist; shows "allowlist full (6)" instead of failing silently. NVS-restored stub shows `rssi --` and `seen prev boot`.
- **Settings auto-save** each change to NVS (toast "saved"); last row is now "Done -> Home", so hold-B no longer loses unsaved edits.
- Header battery shows `--%` when the PMU read fails (was a misleading red `0%`) and a green `+` while charging.
- Probe results are snapshotted under the RX-callback spinlock before upsert.
- About: replaced a UTF-8 em dash the built-in font couldn't render.

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
