#pragma once

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

/*
 * crab'IT Smart Aquaculture — ESP32-S3-DevKitC-1 pin map
 *
 * Fill sensor GPIOs for THIS board. Do NOT copy from smart-aquaculture-firmware
 * unless you verify the same pins on the DevKitC-1.
 */

/* ---------- Board identity ---------- */
#define BOARD_NAME              "ESP32-S3-DevKitC-1-N16R8"
#define FIRMWARE_NAME           "crab'IT"

/* ---------- Sensor enable (1 = wired on this board) ---------- */
#define SENSOR_TEMP_ENABLED     1
#define SENSOR_TURBIDITY_ENABLED 1
#define SENSOR_WATER_LEVEL_ENABLED 1

/*
 * ESP32-S3 ADC1: GPIO4→CH3, GPIO5→CH4, GPIO6→CH5
 */
#define SENSOR_ADC_UNIT          ADC_UNIT_1

#define WATER_LEVEL_GPIO         GPIO_NUM_4
#define WATER_LEVEL_ADC_CHANNEL  ADC_CHANNEL_3

#define TURBIDITY_GPIO           GPIO_NUM_5
#define TURBIDITY_ADC_CHANNEL    ADC_CHANNEL_4

#define TEMP_GPIO                GPIO_NUM_6
#define TEMP_ADC_CHANNEL         ADC_CHANNEL_5

/* HW-038 style level probe: power from 3.3V rail (not GPIO) */
#define WATER_LEVEL_POWER_FROM_GPIO 0
#define WATER_LEVEL_POWER_GPIO      GPIO_NUM_NC

/* NTC 10k B3950 + 10k series divider */
#define NTC_R_SERIES_OHMS        10000.0f
#define NTC_R0_OHMS              10000.0f
#define NTC_T0_KELVIN            298.15f
#define NTC_BETA                 3950.0f
#define ADC_VREF_MV              3300

/*
 * Divider wiring:
 *   1 = 3.3V — 10k series — ADC(GPIO6) — NTC — GND   (most common)
 *   0 = 3.3V — NTC — ADC(GPIO6) — 10k series — GND
 * If room-temp reading is wildly wrong (~50°C in air), flip this.
 */
#define NTC_DIVIDER_NTC_TO_GND   1

/* ADC noise reduction */
#define ADC_OVERSAMPLE_COUNT     32
#define ADC_SAMPLE_GAP_US        300
/* EMA: lower = smoother (0.10–0.25 recommended) */
#define TEMP_EMA_ALPHA           0.15f
/* Reject single-sample voltage jumps larger than this (loose wire / EMI) */
#define TEMP_MAX_STEP_V          0.35f

/* Minimum ADC voltage to treat sensor as electrically connected */
#define SENSOR_CONNECTED_MIN_V   0.05f

/* Sample period */
#define SENSOR_SAMPLE_MS         1000

/* ========== CALIBRATION (fill during bench setup) ==========
 * Print "CAL |" lines on serial / use dashboard volts while calibrating.
 * See guide in chat / README calibration section.
 */

/* 1) Temperature: ESP_shown + OFFSET ≈ true thermometer */
#define CAL_TEMP_OFFSET_C        0.0f

/* 2) Water level — piecewise voltage → % bands
 *   0.000 V              → DRY   0%
 *   0.500–1.300 V        → LOW   1–30%
 *   1.400–1.500 V        → MED   30–70%
 *   1.600–1.800 V        → FULL  70–100%
 *   >1.800 V             → OVER (+ alert: possible new item in basket)
 */
#define CAL_LEVEL_V_DRY            0.000f
#define CAL_LEVEL_V_LOW_START      0.500f
#define CAL_LEVEL_V_LOW_END        1.300f
#define CAL_LEVEL_V_MED_START      1.400f
#define CAL_LEVEL_V_MED_END        1.500f
#define CAL_LEVEL_V_FULL_START     1.600f
#define CAL_LEVEL_V_FULL_END       1.800f
#define ALERT_LEVEL_OVER_V         CAL_LEVEL_V_FULL_END

/* 3) Turbidity (higher V = clearer)
 * Bench (user):
 *   clean water ~1.90 V → CLEAR
 *   light coffee ~1.86–1.87 V → CLOUDY
 *   coffee ~1.62–1.64 V → DIRTY
 *   air ~1.60–1.69 V ≈ dirty (probe should be in water)
 */
#define CAL_TURB_V_CLEAR         1.88f   /* >= → CLEAR */
#define CAL_TURB_V_CLOUDY        1.75f   /* >= and < CLEAR → CLOUDY */
#define CAL_TURB_V_DIRTY         1.70f   /* < → DIRTY alert */

/* Alert bands (crayfish / Electric Blue friendly defaults) */
#define ALERT_TEMP_LOW_C         20.0f
#define ALERT_TEMP_HIGH_C        30.0f
#define ALERT_LEVEL_LOW_PCT      30
#define ALERT_TURB_DIRTY_V       CAL_TURB_V_DIRTY

/* 1 = print CAL | lines every sample (handy while calibrating) */
#define CALIBRATION_LOG          1

/* ---------- Irrigation pump (YX DC12V via SRD-05VDC relay) ---------- */
#define PUMP_GPIO                GPIO_NUM_16
/*
 *   Buck IN+ ← 12V+ ; Buck IN- ← 12V-
 *   Buck OUT+ → relay DC+ ; Buck OUT- → common GND (ESP GND + 12V-)
 *   Relay DC- → common GND
 *   Relay IN  → GPIO16 ; jumper L (required for 3.3V ESP)
 *   COM ← 12V+ ; NO → pump+ ; pump- → 12V- rail (not ESP GND pin)
 *   OFF uses Hi-Z (not 3.3V high) so 5V optocoupler fully releases.
 */
#define PUMP_ACTIVE_LOW          1
/* On boot: dual-polarity click test so you can verify the relay */
#define PUMP_BOOT_SELFTEST       1
/* Hard safety cap — pump auto-stops even if command stays ON */
#define PUMP_MAX_RUN_MS          120000
