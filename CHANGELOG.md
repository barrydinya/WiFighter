## v1.6.0

**Finish claimed persistence + OUI**
- Wired `ouiLookup` to the full table in `oui.h` (was a 2-entry stub).
- Implemented rate-limited last-left NVS save/restore (was a no-op stub). Home now shows the previous session's last LEFT device after reboot.
- Dirty flag + flush on power tick / before potential sleep.
- Still fully passive / listen-only. No deauth, association, injection, or GATT connect.
- Ready to flash on M5StickC Plus / Plus2.

# Changelog

## v1.5.5

**Core restore for release:** tip of main had `wf_core.h` reduced to a placeholder, so the sketch would not build. Full core restored (device table, leftover detection, rate-limited last-left NVS, power/sleep, allowlist, helpers). Home and Menu (already complete in `wf_ui.h`) now build and run.

- Device table (MAX_DEV=28), probe slots, 6-slot allowlist in NVS
- Rate-limited last-left persistence (1/min) + stub restore
- Cooperative scan phases remain non-blocking
- Battery icon, sticky error banner, sweep ring, live menu counts unchanged
- Still fully passive / listen-only
- Ready to flash on M5StickC Plus / Plus2

## Previous

See prior entries for UI polish, rate-limiting, and earlier restores.
