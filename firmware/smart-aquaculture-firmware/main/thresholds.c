#include "thresholds.h"

const char *sensor_state_str(sensor_state_t state)
{
    switch (state) {
    case SENSOR_ACTIVE:
        return "ACTIVE";
    case SENSOR_CONNECTED:
        return "CONNECTED";
    default:
        return "OFF";
    }
}

param_band_t thresholds_temp_level(float celsius, bool valid)
{
    if (!valid) {
        return (param_band_t){ PARAM_LEVEL_LOW, "NO DATA", "#64748b" };
    }
    if (celsius < 22.0f) {
        return (param_band_t){ PARAM_LEVEL_LOW, "LOW", "#3b82f6" };
    }
    if (celsius <= 30.0f) {
        return (param_band_t){ PARAM_LEVEL_MEDIUM, "MEDIUM", "#22c55e" };
    }
    return (param_band_t){ PARAM_LEVEL_HIGH, "HIGH", "#ef4444" };
}

param_band_t thresholds_turbidity_level(float volts)
{
    /*
     * TS-300B: lower voltage = cloudier/dirtier water.
     * LOW    = clean  (high V)
     * MEDIUM = cloudy
     * HIGH   = dirty  (low V) — needs attention
     */
    if (volts >= 2.0f) {
        return (param_band_t){ PARAM_LEVEL_LOW, "LOW", "#22c55e" };
    }
    if (volts >= 1.0f) {
        return (param_band_t){ PARAM_LEVEL_MEDIUM, "MEDIUM", "#f59e0b" };
    }
    if (volts >= 0.08f) {
        return (param_band_t){ PARAM_LEVEL_HIGH, "HIGH", "#ef4444" };
    }
    return (param_band_t){ PARAM_LEVEL_LOW, "NO DATA", "#64748b" };
}

param_band_t thresholds_water_level(int percent)
{
    if (percent <= 5) {
        return (param_band_t){ PARAM_LEVEL_LOW, "DRY", "#ef4444" };
    }
    if (percent <= 40) {
        return (param_band_t){ PARAM_LEVEL_LOW, "LOW", "#f59e0b" };
    }
    if (percent <= 75) {
        return (param_band_t){ PARAM_LEVEL_MEDIUM, "MEDIUM", "#22c55e" };
    }
    return (param_band_t){ PARAM_LEVEL_HIGH, "HIGH", "#3b82f6" };
}
