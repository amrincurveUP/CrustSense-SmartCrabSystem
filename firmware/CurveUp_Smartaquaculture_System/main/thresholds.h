#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    PARAM_LEVEL_LOW = 0,
    PARAM_LEVEL_MEDIUM,
    PARAM_LEVEL_HIGH,
} param_level_t;

typedef struct {
    param_level_t level;
    const char *label;
    const char *color;
} param_band_t;

param_band_t thresholds_temp_level(float celsius, bool valid);
param_band_t thresholds_turbidity_level(float volts);
param_band_t thresholds_water_level(int percent);

const char *water_level_label(int percent);

/* Writes JSON array into buf; returns alert count */
int thresholds_collect_alerts(float temp_c, bool temp_ok, float turb_v, int level_pct,
                              float level_v, char *buf, size_t len);
