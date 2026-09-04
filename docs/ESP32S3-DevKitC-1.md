# ESP32-S3 DevKitC-1 — Board tab summary

**Module:** ESP32-S3-DevKitC-1 **N16R8** (16 MB flash, 8 MB octal PSRAM)  
**Firmware project:** `firmware/CurveUp_Smartaquaculture_System`  
**Device ID:** `curveup-s3-1`  
**Status:** Live on WiFi → HTTP ingest → dashboard (USB not required for readings)

---

## Why this board / project

Old `smart-aquaculture-firmware` ran on a duplicate/clone brain board and gave bad sensor values.  
**New stack only** — do not mix pins or flash the old project onto this DevKitC-1.

---

## Hardware wiring

| Sensor | GPIO | ADC | Notes |
|--------|------|-----|--------|
| Water level | **4** | ADC1_CH3 | Analog probe; VCC + GND + signal |
| Turbidity | **5** | ADC1_CH4 | Higher V ≈ clearer water |
| NTC 10k + 10k divider | **6** | ADC1_CH5 | Temp; offset calibration pending (thermometer) |

Power for field use: **USB wall adapter** (not laptop USB). Laptop USB is for flash/debug only.

### Irrigation relay (SRD-05VDC)

| Relay pin | Connect to |
|-----------|------------|
| D+ / VCC | **5V** — prefer a **5V buck from the 12V adapter** (not ESP 3.3V) |
| D− / GND | Common GND with ESP + 12V− |
| IN | GPIO **17**, jumper **L** |
| COM / NO | 12V+ → COM, NO → pump+; 12V− → pump− |

**Do not power the coil from 3.3V** — LEDs may light, but the relay often will not click.  
If the ESP header “5V” pin does not light the module’s power LED, use an external 5V buck instead.

---

## Software stack

```
Sensors → ESP32-S3 (ESP-IDF)
       → WiFi STA
       → POST http://<Mac-IP>:3000/api/ingest  (every 5s)
       → Node backend + SQLite
       → Dashboard GET /api/readings (every 2s)
```

| Piece | Path / URL |
|--------|------------|
| Firmware | `CurveUp_Smartaquaculture_System` |
| Backend | `backend/` — start: `backend/start.sh` |
| Dashboard | http://127.0.0.1:3000/ or http://\<Mac-LAN-IP\>:3000/ |
| Config host | `main/mqtt_config.h` → `CLOUD_HOST` / ingest URL |

MQTT code exists but is **off** (`MQTT_ENABLED 0`); macOS firewall blocked `:1883`. HTTP ingest is the live path.

---

## Calibrations locked in

### Water level (piecewise)

| Voltage | Band | % |
|---------|------|---|
| &lt; 0.500 V | DRY | 0% |
| 0.500–1.300 V | LOW | 1–30% |
| 1.400–1.500 V | MEDIUM | 30–70% |
| 1.600–1.800 V | FULL | 70–100% |
| &gt; 1.800 V | OVER | &gt;100% + alert (possible new item in basket) |

Gaps (e.g. 1.300–1.400, 1.500–1.600) hold the edge %.

### Turbidity (cup tests)

| Condition | ~Voltage | Band |
|-----------|----------|------|
| Clean water | ~1.90 V | CLEAR (≥ 1.88 V) |
| Light coffee | ~1.86–1.87 V | CLOUDY (1.75–1.88 V) |
| Coffee | ~1.62–1.64 V | DIRTY (&lt; 1.75 V); alert &lt; 1.70 V |
| Air | ~1.60–1.69 V | Looks dirty — keep probe in water |

### Temperature

Responds correctly to cold/hot water; **absolute offset not calibrated yet** (needs thermometer).  
`CAL_TEMP_OFFSET_C` still `0.0`. Alerts: &lt; 20°C / &gt; 30°C.

Firmware also uses ADC oversampling, EMA, and spike reject on NTC.

---

## Alerts (JSON + serial)

| Code | Meaning |
|------|---------|
| `TEMP_LOW` / `TEMP_HIGH` | Outside 20–30°C |
| `LEVEL_LOW` | Below 30% |
| `LEVEL_OVER` | Above 1.800 V — level rose / something added |
| `TURB_DIRTY` | Turbidity voltage below dirty threshold |

---

## What happens if USB is unplugged

- **USB to laptop only:** board loses power → no new data (unless powered by wall USB).
- **Realtime path does not use USB** — WiFi + Mac backend + browser.
- Unplug laptop USB **after** plugging ESP into a wall charger; leave Mac on with backend running.

---

## Quick ops

```bash
# Backend (Mac) — installs LaunchAgent + KeepAlive; advertises crabit.local
/Users/istore/Documents/SmartAquaculture-IoT/backend/start.sh

# Prefer this URL (survives Mac DHCP IP changes):
open http://crabit.local:3000/

# Flash (only when USB connected for programming)
cd firmware/CurveUp_Smartaquaculture_System
export IDF_PYTHON_ENV_PATH="$HOME/.espressif/python_env/idf5.5_py3.13_env"
source ~/esp/esp-idf/export.sh
idf.py -p /dev/cu.usbmodem* flash
```

Firmware talks to **`crabit.local`** (Bonjour / crab'IT), not a raw IP — so DHCP IP changes no longer require a reflash.
Backend auto-starts at login via LaunchAgent `com.curveup.backend`.

---

## Next steps

1. Temperature calibration with thermometer (`CAL_TEMP_OFFSET_C`)
2. Confirm basket install readings (level / turb / temp stable)
3. Remote access when away (tunnel / always-on Pi or VPS)
4. ESP offline buffer if Mac is off
5. L2 / L3 tanks, history charts, mobile alerts

---

## Chat timeline (condensed)

1. New project for DevKitC-1 (not old firmware clone)  
2. Target S3 + N16R8; pins GPIO 4/5/6  
3. Flash bring-up; download-mode / port issues resolved  
4. WiFi + HTTP dashboard sync (MQTT blocked by firewall)  
5. NTC noise filtering  
6. Level + turbidity bench calibration + OVER/DIRTY alerts  
7. Always-on backend script; clarified no-USB realtime over WiFi  
