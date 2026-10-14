# Changelog

## v1.4.0
* Expanded offline OUI vendor database (more Intel, Dell, HP, Samsung, virtualization, etc.)
* Home header battery icon + percentage for quicker glance
* Minor UI polish on home dashboard and menu
* Still fully passive: no deauth, no association, no GATT connect
* Home screen and menu remain the primary field UI

## v1.3.1
Production rebuild: restore truncated `wf_core.h` so the sketch links and runs.

* Device table helpers: upsert, markLeft, allowlist, NVS prefs load/save
* Counters and left-first list indexing for Devices / Alerts
* BLE company-ID tags (Apple / Samsung / Google) + Tile name heuristic
* Correct NimBLE native MAC byte order for display and matching
* Home uptime, cycle serial log, PlatformIO builds only WiFighter.ino
* Still passive: no deauth, no association, no GATT connect

## v1.3.0
* Home polish pass (sweep ring, last-left card, live paint)

## v1.2.0
Home screen and menu are the release UI.

* Home: sweep ring with AP / probe / BLE phase ticks, radio flags, next-scan line, cycle bar, last-left card, DEV / IN / LEFT / HIGH / PRB tiles
* Menu: eight items, glyph badges, live counts, hint line, A opens, B advances, hold B returns home
* Menu repaints while open so scan state and counts stay current
* Still passive: no deauth, no association, no GATT connect

## v1.1.0
* Home dashboard and menu first pass
* Allowlist screen can remove a saved MAC

## v1.0.0
* Device table, leftover window, probe hop, BLE advert scan
