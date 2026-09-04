# CurveUp Smart Aquaculture System

New ESP-IDF firmware for **ESP32-S3-DevKitC-1** and new sensors.  
Separate from `smart-aquaculture-firmware` (do not mix the two).

## Status

- Target: **esp32s3** — ESP32-S3-DevKitC-1 **N16R8** (16 MB flash, 8 MB PSRAM)
- Sensors wired:
  - **GPIO 4** — water level (ADC1_CH3)
  - **GPIO 5** — turbidity (ADC1_CH4)
  - **GPIO 6** — NTC 10k + 10k divider (ADC1_CH5)

## Still needed

1. **Basket layout** — L1 / L2 / L3 species and which tanks have sensors
2. **WiFi** — fill `main/wifi_config.h` when ready

## Dashboard sync path

```
ESP32 (WiFi) → MQTT :1883 → backend :3000 → dashboard HTML
```

| Setting | Value |
|---------|--------|
| Device ID | `curveup-s3-1` |
| MQTT broker | `mqtt://192.168.1.6:1883` (edit `main/mqtt_config.h` if Mac IP changes) |
| Readings topic | `smart-aquaculture/curveup-s3-1/readings` |
| Dashboard | http://192.168.1.6:3000/ |
| API | http://192.168.1.6:3000/api/readings |

Start broker + API on your Mac (Docker optional; Homebrew Mosquitto works):

```bash
# Mosquitto (LAN listen)
/opt/homebrew/opt/mosquitto/sbin/mosquitto -c /opt/homebrew/etc/mosquitto/mosquitto.conf &

# Backend + dashboard
cd backend
MQTT_URL=mqtt://127.0.0.1:1883 node src/index.js
```

If ESP logs `Connection reset by peer` on MQTT: allow Mosquitto through **macOS Firewall** (System Settings → Network → Firewall), or temporarily turn firewall off.

## Quick start

```bash
cd firmware/CurveUp_Smartaquaculture_System
export IDF_PYTHON_ENV_PATH="$HOME/.espressif/python_env/idf5.5_py3.13_env"
source ~/esp/esp-idf/export.sh

idf.py build
idf.py -p /dev/cu.usbmodem* flash monitor
```

## Project layout

```
main/
  main.c              — app entry
  board_config.h      — GPIO / ADC / thresholds (EDIT THIS)
  wifi_config.h       — WiFi SSID (EDIT THIS)
  sensor_adc.*        — oneshot ADC helper
  sensors.*           — temperature / turbidity / level readers
  sensor_service.*    — FreeRTOS task + JSON snapshot
```

## Note

Old project: `firmware/smart-aquaculture-firmware`  
New project: `firmware/CurveUp_Smartaquaculture_System` ← use this one only
