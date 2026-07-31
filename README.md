# WiFighter

**Portable WiFi + BLE device data recon tool for M5StickC Plus / Plus2**

WiFighter scans nearby WiFi access points and BLE advertisements to collect device identifiers, signal strength, encryption types, manufacturer data and residual network information. It also tracks **devices that have left** the current scan window (“ghosts”) and includes a continuous **probe-request sniffer** so you can see clients still looking for networks they previously associated with.

Intended for **authorized penetration testing, security research and education** only.

> **Legal notice**  
> This is for authorized penetration testing, security research, and educational purposes on systems you own or have explicit written permission to test. Unauthorized access or disruption of networks/systems is illegal under laws like the US Computer Fraud and Abuse Act and similar regulations worldwide. Respect scope, obtain permissions, and avoid impacting production or third-party systems.

## Features (v0.6)

- Clean **Home Screen** with live battery %, status, WiFi/BLE/probe counts, ghost count, last-scan age
- Full **Main Menu** (button driven, scrollable)
- **WiFi Recon** – SSID, BSSID, RSSI, channel, encryption (sorted by strength + RSSI bar)
- **BLE Recon** – name, MAC, RSSI, manufacturer data (deduplicated by MAC, sorted + RSSI bar)
- **Hybrid Scan** – sequential WiFi + BLE with combined scrollable results
- **Probe Sniff** (new) – continuous promiscuous capture of probe requests (devices seeking left networks), channel hop 1-13, unique MAC + SSID + RSSI
- **Ghosts / Left** – devices seen in a previous scan that are no longer present
- **Export Serial** – dump full results table + ghosts over USB Serial
- **Settings** (scan duration, brightness, auto-sleep) with NVS persistence
- Optional auto deep-sleep after 3 min idle
- Clear data option (results + ghosts + probes)
- Power-aware UI
- NimBLE for efficient BLE scanning
- Simple navigation: A = Select / Back, B = Next / Scroll / Clear, Power = Home

## Hardware

- M5StickC Plus or **M5StickC Plus2** (recommended – better RF performance)
- No extra modules required for basic operation

## Software Requirements

1. Arduino IDE 2.x
2. Board package: M5Stack (or Espressif ESP32) – select **M5Stick-C-Plus** or **M5StickC Plus2**
3. Libraries (Library Manager):
   - `M5Unified`
   - `M5GFX`
   - `NimBLE-Arduino` (by h2zero)

## Quick Start

1. Clone / download this repo
2. Open `WiFighter.ino` in Arduino IDE
3. Select the correct board and port
4. Upload
5. On device:
   - **A (front)** = Select / Enter / Back to menu
   - **B (side)** = Next item / Scroll results / Clear probes
   - **Power button** (short) ≈ Home

## Controls Summary

| Screen       | A                  | B              | Power     |
|--------------|--------------------|----------------|-----------|
| Home         | Open Menu          | -              | -         |
| Menu         | Select item        | Next item      | Home      |
| Results      | Back to Menu       | Scroll         | Home      |
| Probe Sniff  | Stop + Back        | Clear log      | Home      |
| Ghosts       | Back to Menu       | Scroll         | Home      |
| Settings     | Change / Save      | Next setting   | Home      |
| About        | Back to Menu       | -              | Home      |

## Roadmap / Next Improvements

- [ ] Persistent log storage of last N scans (Preferences or SD via Grove)
- [x] Probe-request focused mode (promiscuous / monitor mode)
- [ ] RSSI proximity alerts / simple graph history
- [ ] Export via BLE GATT characteristic
- [ ] Companion Arduino 32-bit board support for SD + GPS logging
- [ ] Integration path with Bruce / NEMO style firmwares
- [x] Ghost / left-device tracking
- [x] Auto deep-sleep after idle timeout
- [x] Serial export table
- [x] BLE MAC deduplication
- [x] Visual RSSI bars
- [x] Home screen + full menu

## Project Status

**v0.6** – Home screen, Menu, WiFi/BLE/Hybrid scanners, continuous Probe Request Sniffer, Ghosts (devices that left), Settings with persistence + auto-sleep, RSSI sorting + bars, BLE dedup, Serial export, last-scan age, and solid navigation are complete and usable in the field for authorized work.

Continue iterating until feature-complete, then tag a formal release.

## License

MIT – use responsibly and only where authorized.

---

Maintained by barrydinya  
Built with assistance from Grok (xAI)
