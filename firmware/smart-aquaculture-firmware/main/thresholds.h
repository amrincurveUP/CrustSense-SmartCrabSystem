#pragma once

#include "sensor_status.h"

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

const char *sensor_state_str(sensor_state_t state);
