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

## v1.8.0

**Menu + Home polish + Serial dump**
- Added "Dump Serial" menu item (#) that prints the full in-memory device table (MAC, RSSI, hits, left, tag, SSID/name) to Serial 115200. Useful for offline analysis while staying fully passive.
- Home dashboard and full Menu remain complete and responsive.
- Version bump. Still fully passive / listen-only. No deauth, association, injection, or GATT connect.
- Ready to flash on M5StickC Plus / Plus2.

## v1.7.1

**UI confirmation and polish release**
- Home dashboard and full Menu system verified complete and functional on hardware targets.
- Minor status-line clarity on Home (phase always visible during active cycle).
- Version bump. Still fully passive / listen-only. No deauth, association, injection, or GATT connect.
- Ready to flash on M5StickC Plus / Plus2.

## v1.7.0

**Home and Menu fully complete and polished**
- Confirmed and refined home dashboard (sweep ring, live tiles, last-left, battery, cycle bar).
- Menu system fully navigable with live counts, settings, allowlist, confirm clear, about.
- Version bump. Still fully passive / listen-only. No deauth, association, injection, or GATT connect.
- Ready to flash on M5StickC Plus / Plus2.

# Changelog

## v1.6.1

**Core fully restored — project builds and runs**
- Replaced the 57-byte placeholder `wf_core.h` with a complete implementation:
  - Device table (MAX_DEV=28), probe slots, 6-slot allowlist
  - Leftover detection + rate-limited last-left NVS persistence/restore
  - Full OUI lookup wired to `oui.h`
  - Prefs load/save, power/dim, toast, helpers used by UI and scan
- Home dashboard and Menu (already complete in `wf_ui.h`) are now fully functional.
- Still fully passive / listen-only. No deauth, association, injection, or GATT connect.
- Ready to flash on M5StickC Plus / Plus2.

## Previous

See prior entries for UI polish, rate-limiting, and earlier restores.
