# ESP Home Network

An ESP32-based local Wi-Fi access point with a browser-based settings page. The initial firmware has been tested on an ESP32-WROOM-32 board selected as **ESP32 Dev Module** in Arduino IDE.

## Current firmware: v1.0

### Features
- Creates a local Wi-Fi network without requiring internet access.
- Hosts a settings page at `http://192.168.4.1`.
- Allows the Wi-Fi SSID and password to be changed from the web page.
- Saves SSID/password in ESP32 Preferences (NVS), so settings survive restarts.
- Configures up to 8 connected stations in the access point configuration.

### Default Wi-Fi credentials
- **SSID:** `ESP Home Network`
- **Password:** `HomeNet@123`

> **Security warning:** Change the default Wi-Fi password before using the device around untrusted people. The v1.0 settings page does not have admin login yet; any connected client can open the settings page and attempt to change the network credentials.

## Hardware
- ESP32-WROOM-32 development board (USB-C board variant)
- USB data cable
- USB power source

No extra electronic components are required for this local-only version.

## Upload firmware
1. Install Arduino IDE.
2. In Boards Manager, install **esp32 by Espressif Systems**.
3. Open `firmware/ESP_Home_Network.ino`.
4. Select **ESP32 Dev Module** under Tools → Board.
5. Select the correct serial port under Tools → Port.
6. Upload the sketch.
7. Optionally open Serial Monitor at **115200 baud** to view startup messages.

## Connect
1. On a phone or laptop, connect to `ESP Home Network`.
2. Enter `HomeNet@123`.
3. Open `http://192.168.4.1` in a browser.

A phone may report “No internet”; that is expected. This device provides a local Wi-Fi network only and does not route internet traffic.

## Current limitations
- This is a local Wi-Fi access point, not a cellular modem or internet router.
- It does not route internet traffic.
- The v1.0 web panel has no authentication. Use it only on a trusted local network.
- The firmware does not currently show connected-device names or provide an admin dashboard.

## Roadmap
- [x] v1.0: Access point and local Wi-Fi settings page
- [ ] v2.0: Admin login and password-change flow
- [ ] v2.0: Connected-device dashboard
- [ ] v2.0: System status and restart controls

## Repository structure

```text
ESP-Home-Network/
├── firmware/
│   └── ESP_Home_Network.ino
└── README.md
```
