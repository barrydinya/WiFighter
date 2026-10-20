# Changelog

## v1.5.0

UI polish: battery icon in header, cleaner home/menu feedback. Still fully passive.

- Header now draws a small battery icon + numeric level (color changes for low/charging).
- Home dashboard and menu remain the primary field UI; minor spacing and visual consistency tweaks.
- Version bump; still no deauth, association, or GATT connect.

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
