# WiFighter

**Portable WiFi + BLE device data recon tool for M5StickC Plus / Plus2**

WiFighter scans nearby WiFi access points and BLE advertisements to collect device identifiers, signal strength and residual network information. It tracks **devices that have left** the current scan window (“ghosts”) so you can still pull data about devices that recently departed the area.

Intended for **authorized penetration testing, security research and education** only.

> **Legal notice**  
> This is for authorized penetration testing, security research, and educational purposes on systems you own or have explicit written permission to test. Unauthorized access or disruption of networks/systems is illegal under laws like the US Computer Fraud and Abuse Act and similar regulations worldwide. Respect scope, obtain permissions, and avoid impacting production or third-party systems.

## Features (v1.1)

- **Polished Home Screen** – live battery, tracked/active/ghost counts with visual bar, BLE vs WiFi split, last-scan age, quick controls
- **Full Main Menu** – button-driven with highlight, 8 entries including dedicated Ghosts view
- **WiFi Scan** – SSID, BSSID, RSSI (hidden networks included)
- **BLE Scan** – name, MAC, RSSI (NimBLE, efficient shared callback)
- **Track Mode** – continuous hybrid monitoring; marks devices that leave range; proximity highlight; tone alert on new ghost
- **Device List** + **Ghosts Only** – filter view of left devices
- **Settings**: Clear all / Clear ghosts only, Serial CSV export, AutoSleep toggle, Brightness control
- **Simple OUI vendor hints** (Apple, Samsung, Espressif, RPi, Intel, ASUS, Google, VMware…)
- **NVS persistence** of tracked devices across reboots (including vendor + best RSSI)
- Power-aware UI with battery colour coding and optional dim on inactivity
- Navigation: **A** = Select / Enter / Back, **B** = Next / Scroll / Quick Track from Home

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
   - **B (side)** = Next item / Scroll / Quick Track from Home

## Project Status

**v1.1** – Complete Home Screen + Menu system, WiFi/BLE scanners, Track Mode with ghost detection + audio alert, dedicated Ghosts list, Settings (clear all / clear ghosts / export / sleep / brightness), NVS persistence with vendor + best RSSI, serial CSV export, proximity highlighting, simple OUI lookup. Ready for authorized field testing.

Possible future iterations:
- Probe-request / promiscuous mode sniffing
- Configurable timeouts via Settings
- SD card or companion board logging
- ESP-NOW mesh of multiple sticks
- More rich advertisement parsing (manufacturer data, service UUIDs)

## Controls Summary

| Button | Context          | Action                  |
|--------|------------------|-------------------------|
| A      | Home             | Open Menu               |
| A      | Menu / Settings  | Select / Execute        |
| A      | Other screens    | Back to Menu            |
| B      | Home             | Jump to Track Mode      |
| B      | Menu / Settings  | Next item               |
| B      | Other screens    | Back to Menu            |

## License

MIT – use responsibly and only where authorized.

---

Maintained by barrydinya  
Built with assistance from Grok (xAI)
