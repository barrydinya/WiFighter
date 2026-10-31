# Changelog

## v1.5.4

**Critical restore (again):** tip of `main` at v1.5.3 shipped a truncated `wf_core.h` containing only the placeholder `PLACEHOLDER_WILL_REPLACE`. The sketch could not build. This release restores the full core sources (device table, leftover detection, rate-limited last-left NVS, power/sleep, allowlist, helpers) from the last complete v1.5.2 tree, keeps the v1.5.3 home-refresh polish, and confirms Home + Menu are complete and functional.

- Restored full `wf_core.h` (678 lines). Home (`drawHome` / `paintHomeLive`) and Menu (`drawMenu` + live counts) in `wf_ui.h` were already present and unchanged.
- Version bumped to 1.5.4.
- Still fully passive / listen-only. No deauth, association, injection, or GATT connect.
- Ready to flash on M5StickC Plus / Plus2.

## v1.5.3

UI polish for Home and Menu (already complete since v1.2/v1.5.1). Still fully passive / listen-only.

- **Smoother home sweep ring**: Home screen now refreshes every ~90 ms (was 400 ms) so the rotating sweep indicator steps much more smoothly. Other screens keep the 400 ms interval to save cycles.
- Ring phase ticks and radio flags remain responsive.
- No functional changes to scanning, leftover detection, or NVS. Compile-checked.

## v1.5.2

Source review of the v1.5.1 tip: **Home and Menu are complete in the source** (`wf_ui.h` `drawHome`/`paintHomeLive`, `drawMenu`, every sub-screen, and `handleA`/`handleB`). No stubs or TODOs. This release fixes two real issues the review turned up. Still fully passive / listen-only.

- **Last-left NVS writes are rate-limited (flash wear + UI stalls).** Every "device left" event used to write the last-left record to NVS right away. In busy areas, phones with randomized MACs cause several leave events per cycle, which meant constant flash writes and short blocking stalls in `loop()`. The record now updates in RAM immediately (Home shows it straight away) and goes to NVS at most once every 60 s (`LL_SAVE_MIN_MS`). Any pending record is also written before deep sleep (idle or PWR hold). A failed write shows the sticky `NVS save fail` banner.
- **Hold B on Home no longer opens the Menu.** Holding B while already on Home used to fall through to `handleB()` on release and open the Menu. The hold is now swallowed on every screen, so hold-B always means "Home".

## v1.5.1

**Critical restore:** tip of `main` at v1.5.0 shipped *truncated* `wf_core.h` (476 B) and `wf_ui.h` (3.5 KB ending in a placeholder comment). Home, Menu, device table, NVS prefs, and power management were wiped — the sketch could not build. This release restores the full sources from the last good v1.4.0 firmware commit and keeps the intended battery-icon polish.

- Restored full `wf_core.h` (device table, leftover detection, NVS prefs, power/sleep, allowlist) and full `wf_ui.h` (Home dashboard + Menu + all screens).
- Header draws a compact battery icon + numeric level (red when low, green when charging; `--` when PMU read fails).
- Confirmed Home: sweep ring, WIFI/BLE/PRB flags, sticky red error banner, cycle bar, last-left card, DEV / IN / LEFT / HIGH / PRB tiles, uptime.
- Confirmed Menu: Scan / Devices / Alerts / Allowlist / Settings / Clear / About / Home with live counts; A open, B next, hold-B Home; non-blocking refresh.
- Still fully passive / listen-only. No deauth, association, injection, or GATT connect.

## v1.5.0

**Broken on GitHub tip** — do not flash. Claimed battery-icon polish, but the push truncated `wf_core.h` / `wf_ui.h`. Superseded by v1.5.1.
