#pragma once

/*
 * Your wiring (ESP32-S3-N16R8)
 *
 * [1] NTC 10k thermistor (B3950)
 *     3.3V -> 10k resistor -> GPIO4 -> probe -> GND
 *
 * [2] Turbidity TS-300B
 *     VCC -> 3.3V (or 5V + divider), GND -> GND, AO -> GPIO7
 *
 * [3] Water level HW-038
 *     S -> GPIO8,  + -> 3.3V,  - -> GND
 */

#define PIN_TEMPERATURE_ADC       4
#define PIN_TURBIDITY_ADC         7
#define PIN_WATER_LEVEL_SIGNAL    8

#define PIN_WATER_LEVEL_POWER     8
#define WATER_LEVEL_POWER_FROM_GPIO  0

#define NTC_R0_OHMS               10000.0f
#define NTC_B_COEFFICIENT         3950.0f
#define NTC_T0_KELVIN             298.15f
#define NTC_SERIES_OHMS           10000.0f

#define ADC_SAMPLE_COUNT          16
#define SENSOR_READ_INTERVAL_MS   2000
#define WATER_LEVEL_POWER_MS      10

#define TEMP_VOLT_ACTIVE_MIN      0.55f
#define TEMP_VOLT_ACTIVE_MAX      2.95f
#define TEMP_VOLT_CONNECTED_MIN   0.05f
#define TEMP_C_ACTIVE_MIN         5.0f
#define TEMP_C_ACTIVE_MAX         45.0f

#define TURBIDITY_VOLT_CONNECTED  0.05f
#define TURBIDITY_VOLT_ACTIVE     0.25f
#define TURBIDITY_VARIATION_ACTIVE 20

#define LEVEL_VOLT_CONNECTED      0.05f
#define LEVEL_VOLT_ACTIVE         0.35f
#define LEVEL_DELTA_CONNECTED     15
#define LEVEL_DELTA_ACTIVE        40
#define LEVEL_DELTA_SWAP_FAULT    -5
#define LEVEL_RAW_FULL_SCALE      2800
