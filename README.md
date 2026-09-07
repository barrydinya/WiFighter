# WiFighter

**Portable WiFi + BLE device data recon tool for M5StickC Plus / Plus2**

WiFighter scans nearby WiFi access points and BLE advertisements to collect device identifiers, signal strength and residual network information. It tracks **devices that have left** the current scan window (“ghosts”).

Intended for **authorized penetration testing, security research and education** only.

> **Legal notice**  
> This is for authorized penetration testing, security research, and educational purposes on systems you own or have explicit written permission to test. Unauthorized access or disruption of networks/systems is illegal under laws like the US Computer Fraud and Abuse Act and similar regulations worldwide. Respect scope, obtain permissions, and avoid impacting production or third-party systems.

## Features (v1.0)

- **Home Screen** with live battery, tracked/active/ghost counts, last-scan age
- Full **Main Menu** (button driven, clear highlight)
- **WiFi Scan** – SSID, BSSID, RSSI
- **BLE Scan** – name, MAC, RSSI (NimBLE, shared callback)
- **Track Mode** – continuous hybrid monitoring; marks devices that leave range; proximity highlight
- **Device List** – view tracked devices (red = left/ghost, yellow = strong/proximity)
- **Settings**: Clear devices, Serial export, AutoSleep toggle, Brightness control
- **NVS persistence** of tracked devices across reboots
- Power-aware UI with battery % and optional dim on inactivity
- Simple navigation: **A** = Select / Enter / Back, **B** = Next / Scroll

## Hardware

- M5StickC Plus or **M5StickC Plus2** (recommended – better RF performance)
- No extra modules required for basic operation

## Software Requirements

1. Arduino IDE 2.x
2. Board package: M5Stack – select **M5Stick-C-Plus** or **M5StickC Plus2**
3. Libraries (Library Manager):
   - `M5Unified`
   - `M5GFX`
   - `NimBLE-Arduino` (by h2zero)

## Quick Start

1. Clone this repo
2. Open `WiFighter.ino` in Arduino IDE
3. Select the correct board and port
4. Upload
5. On device:
   - **A (front)** = Select / Enter / Back to menu
   - **B (side)** = Next item / Scroll

## Project Status

**v1.0** – Home screen + complete Menu system, WiFi/BLE scanners, Track Mode with ghost detection, Device List, Settings (clear / export / sleep / brightness), NVS persistence, serial CSV export, proximity highlighting. Ready for field testing under authorization.

Possible future iterations:
- Probe-request / promiscuous mode sniffing
- Stronger proximity alerts (beep / flash)
- Configurable timeouts via Settings
- SD card or companion board logging
- ESP-NOW mesh of multiple sticks

## Controls Summary

| Button | Context          | Action                  |
|--------|------------------|-------------------------|
| A      | Home             | Open Menu               |
| A      | Menu / Settings  | Select / Execute        |
| A      | Other screens    | Back to Menu            |
| B      | Menu / Settings  | Next item               |
| B      | Other screens    | Back to Menu            |

## License

MIT – use responsibly and only where authorized.

---

Maintained by barrydinya  
Built with assistance from Grok (xAI)
