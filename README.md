# ESP8266 Relay Control with MQTT and Web Interface

This project implements a smart relay controller using an ESP8266, featuring MQTT integration and a responsive web interface for remote control. The device supports Last Will and Testament (LWT) for connection status monitoring, automatic reconnection, and configuration via WiFiManager.

## Features

- **Relay Control**: Control a relay via GPIO pin (D1).
- **MQTT Integration**: 
  - Publish/subscribe to topics for remote control (`home/relay/control`, `home/relay/status`).
  - LWT messages (`home/relay/online`) to track device connectivity.
  - Heartbeat and diagnostics (RSSI, heap memory) every 30 seconds.
- **Web Interface**:
  - Responsive UI with real-time status updates.
  - REST API endpoints (`/relay/on`, `/relay/off`, `/relay/status`).
  - Configuration reset and device restart options.
- **WiFiManager**: Easy setup for WiFi and MQTT credentials via captive portal.
- **Persistent Storage**: Saves MQTT settings to EEPROM.

## Hardware Requirements

- ESP8266 (e.g., NodeMCU, Wemos D1 Mini)
- Relay module (compatible with 3.3V logic)
- Power supply (5V for ESP8266, relay-specific voltage)

## Configuration

1. **WiFi and MQTT Setup**:
   - On first boot, connect to the `Relay-Setup` AP.
   - Configure WiFi and MQTT settings (server, username, password) via the web portal.
   - Settings are saved to EEPROM for future reboots.

2. **MQTT Topics**:
   - **Control**: `home/relay/control` (send `ON`/`OFF`/`RESTART`).
   - **Status**: `home/relay/status` (publishes `ON`/`OFF`).
   - **Online Status**: `home/relay/online` (LWT messages: `ONLINE`/`OFFLINE`).

3. **Web Endpoints**:
   - `/relay/on`: Turn relay ON.
   - `/relay/off`: Turn relay OFF.
   - `/restart`: Reboot the device.
   - `/reset`: Erase all settings and restart.

## Installation

1. **Arduino IDE Setup**:
   - Install ESP8266 board support via Boards Manager.
   - Install required libraries:
     - `ESP8266WiFi`
     - `ESP8266WebServer`
     - `WiFiManager`
     - `PubSubClient`
     - `EEPROM`

2. **Upload Code**:
   - Compile and upload `esp8266_relay_mqtt_restart_LAST_ver3.3_with_comments.ino` to your ESP8266.

3. **Hardware Connection**:
   - Connect the relay to pin `D1` (GPIO5).
   - Ensure proper power supply for both ESP8266 and relay.

## Usage

- **Web Interface**: Access via `http://<ESP_IP>` to control the relay and view status.
- **MQTT Commands**: Publish to `home/relay/control` with payload `ON`/`OFF`/`RESTART`.
- **Auto-Reconnect**: The device automatically reconnects to WiFi/MQTT if the connection is lost.

## Troubleshooting

- **Check Serial Monitor**: Debug output at `115200 baud`.
- **Reset Settings**: Use the "Reset Settings" button in the web interface or hold the ESP8266 boot button during startup to force AP mode.
- **MQTT Issues**: Verify server IP, credentials, and firewall settings.

## EEPROM Memory Map:
Address---Size----Field----------Description

0----------40------mqtt_server---IP or host of the MQTT broker

40---------40------mqtt_user-----User for MQTT

80---------32------mqtt_pass-----Password


## License

This project is open-source under the MIT License. Feel free to modify and distribute.

---

For detailed code documentation, refer to the inline comments in the `.ino` file.

This README.md provides a comprehensive overview of the project, including setup instructions, features, and usage examples. You can customize it further based on specific hardware details or additional functionalities.# eESP8266-Relay-MQTT-Relay
