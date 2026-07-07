#pragma once

/*
 * Tank 1 prototype wiring (ESP32-S3-N16R8)
 *
 * TS-300B turbidity:
 *   VCC -> 5V, GND -> GND, AO -> voltage divider -> GPIO4
 *   Divider: 100k (AO to GPIO) + 220k (GPIO to GND)
 *
 * NTC 10k stainless probe:
 *   3.3V -> 10k fixed resistor -> GPIO5 -> probe -> GND
 *
 * HW-038 water level:
 *   S  -> GPIO7,  + -> GPIO8 (power enable),  - -> GND
 */

#define PIN_TURBIDITY_ADC       4
#define PIN_TEMPERATURE_ADC     5
#define PIN_WATER_LEVEL_ADC     7
#define PIN_WATER_LEVEL_POWER   8

/* TS-300B divider: scales 0-4.5 V sensor output to ADC-safe range */
#define TURBIDITY_DIVIDER_R_TOP_OHMS     100000.0f
#define TURBIDITY_DIVIDER_R_BOTTOM_OHMS  220000.0f

/* NTC 10k @ 25 C, B3950 — adjust if your probe datasheet differs */
#define NTC_R0_OHMS          10000.0f
#define NTC_B_COEFFICIENT    3950.0f
#define NTC_T0_KELVIN        298.15f
#define NTC_SERIES_OHMS      10000.0f

#define ADC_SAMPLE_COUNT     20
#define SENSOR_READ_INTERVAL_MS  3000
#define WATER_LEVEL_POWER_ON_MS  15
