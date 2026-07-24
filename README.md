# WiFighter

**Portable WiFi + BLE device data recon tool for M5StickC Plus / Plus2**

WiFighter scans nearby WiFi access points and BLE advertisements to collect device identifiers, signal strength, encryption types, manufacturer data and residual network information. It is intended for **authorized penetration testing, security research and education** only.

> **Legal notice**  
> This is for authorized penetration testing, security research, and educational purposes on systems you own or have explicit written permission to test. Unauthorized access or disruption of networks/systems is illegal under laws like the US Computer Fraud and Abuse Act and similar regulations worldwide. Respect scope, obtain permissions, and avoid impacting production or third-party systems.

## Features (v0.1)

- Clean Home Screen with battery status and live counts
- Simple button-driven Main Menu
- WiFi Recon – SSID, BSSID, RSSI, channel, encryption
- BLE Recon – name, MAC, RSSI, manufacturer data preview
- Hybrid scan path (WiFi then BLE)
- Scrollable result views
- Power-aware UI (battery % in status bar)
- NimBLE for efficient BLE scanning

## Hardware

- M5StickC Plus or **M5StickC Plus2** (recommended – better RF)
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
   - **A (front)** = Select / Enter menu
   - **B (side)** = Next item / Scroll results
   - **Power button** (short) ≈ Home / Back (behavior depends on Plus vs Plus2)

## Roadmap / Next Improvements

- [ ] Persistent log storage (Preferences or SD via Grove)
- [ ] Probe-request focused mode (monitor mode experiments)
- [ ] RSSI graph / proximity alerts
- [ ] Export via serial or BLE GATT
- [ ] Settings screen (scan duration, channel filter, power save)
- [ ] Companion Arduino 32-bit board support for SD + GPS logging
- [ ] Integration path with Bruce / NEMO style firmwares

## Project Status

**Active development** – Home screen + Menu + core scanners are implemented.  
Continue iterating until feature-complete, then tag a release.

## License

MIT – use responsibly and only where authorized.

---

Maintained by barrydinya  
Built with assistance from Grok (xAI)
