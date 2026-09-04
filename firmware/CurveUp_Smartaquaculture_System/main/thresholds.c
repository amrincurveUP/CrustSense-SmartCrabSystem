#include "thresholds.h"

#include "board_config.h"
#include <stdio.h>

param_band_t thresholds_temp_level(float celsius, bool valid)
{
    if (!valid) {
        return (param_band_t){ PARAM_LEVEL_LOW, "NO DATA", "#64748b" };
    }
    if (celsius < ALERT_TEMP_LOW_C) {
        return (param_band_t){ PARAM_LEVEL_LOW, "LOW", "#3b82f6" };
    }
    if (celsius <= ALERT_TEMP_HIGH_C) {
        return (param_band_t){ PARAM_LEVEL_MEDIUM, "OK", "#22c55e" };
    }
    return (param_band_t){ PARAM_LEVEL_HIGH, "HIGH", "#ef4444" };
}

param_band_t thresholds_turbidity_level(float volts)
{
    /* Lower voltage = dirtier (calibrated bands) */
    if (volts >= CAL_TURB_V_CLEAR) {
        return (param_band_t){ PARAM_LEVEL_LOW, "CLEAR", "#22c55e" };
    }
    if (volts >= CAL_TURB_V_CLOUDY) {
        return (param_band_t){ PARAM_LEVEL_MEDIUM, "CLOUDY", "#f59e0b" };
    }
    if (volts >= 0.05f) {
        return (param_band_t){ PARAM_LEVEL_HIGH, "DIRTY", "#ef4444" };
    }
    return (param_band_t){ PARAM_LEVEL_LOW, "NO DATA", "#64748b" };
}

param_band_t thresholds_water_level(int percent)
{
    if (percent > 100) {
        return (param_band_t){ PARAM_LEVEL_HIGH, "OVER", "#a855f7" };
    }
    if (percent <= 0) {
        return (param_band_t){ PARAM_LEVEL_LOW, "DRY", "#ef4444" };
    }
    if (percent <= 30) {
        return (param_band_t){ PARAM_LEVEL_LOW, "LOW", "#f59e0b" };
    }
    if (percent <= 70) {
        return (param_band_t){ PARAM_LEVEL_MEDIUM, "MEDIUM", "#22c55e" };
    }
    return (param_band_t){ PARAM_LEVEL_HIGH, "FULL", "#3b82f6" };
}

const char *water_level_label(int percent)
{
    if (percent > 100) {
        return "OVER";
    }
    if (percent <= 0) {
        return "DRY";
    }
    if (percent <= 30) {
        return "LOW";
    }
    if (percent <= 70) {
        return "MEDIUM";
    }
    return "FULL";
}

int thresholds_collect_alerts(float temp_c, bool temp_ok, float turb_v, int level_pct,
                              float level_v, char *buf, size_t len)
{
    if (buf == NULL || len < 8) {
        return 0;
    }

    size_t pos = 0;
    int count = 0;
    buf[0] = '[';
    pos = 1;

    #define APPEND_ALERT(code, msg) do { \
        int n = snprintf(buf + pos, len - pos, \
            "%s{\"code\":\"%s\",\"message\":\"%s\"}", \
            count ? "," : "", code, msg); \
        if (n > 0 && (size_t)n < len - pos) { pos += (size_t)n; count++; } \
    } while (0)

    if (temp_ok && temp_c < ALERT_TEMP_LOW_C) {
        APPEND_ALERT("TEMP_LOW", "Water too cold");
    }
    if (temp_ok && temp_c > ALERT_TEMP_HIGH_C) {
        APPEND_ALERT("TEMP_HIGH", "Water too warm");
    }
    if (level_v > ALERT_LEVEL_OVER_V) {
        APPEND_ALERT(
            "LEVEL_OVER",
            "Water level rose above normal — something may have been added to the basket");
    }
    if (level_pct > 0 && level_pct < ALERT_LEVEL_LOW_PCT) {
        APPEND_ALERT("LEVEL_LOW", "Water level low");
    }
    if (turb_v > 0.05f && turb_v < ALERT_TURB_DIRTY_V) {
        APPEND_ALERT("TURB_DIRTY", "Water turbidity high");
    }

    if (pos + 1 < len) {
        buf[pos++] = ']';
        buf[pos] = '\0';
    }
    return count;
}
