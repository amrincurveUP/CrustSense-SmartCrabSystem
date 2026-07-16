#include "telemetry.h"

#include <math.h>
#include <stdio.h>

static const char *state_str(sensor_state_t state)
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

static int count_connected(const sensor_snapshot_t *snap)
{
    int n = 0;
    if (snap->turbidity != SENSOR_NOT_CONNECTED) {
        n++;
    }
    if (snap->temperature != SENSOR_NOT_CONNECTED) {
        n++;
    }
    if (snap->water_level != SENSOR_NOT_CONNECTED) {
        n++;
    }
    return n;
}

static int count_active(const sensor_snapshot_t *snap)
{
    int n = 0;
    if (snap->turbidity == SENSOR_ACTIVE) {
        n++;
    }
    if (snap->temperature == SENSOR_ACTIVE) {
        n++;
    }
    if (snap->water_level == SENSOR_ACTIVE) {
        n++;
    }
    return n;
}

void telemetry_print_json(const sensor_snapshot_t *snap)
{
    if (snap == NULL) {
        return;
    }

    const int connected = count_connected(snap);
    const int active = count_active(snap);

    if (snap->temperature_valid && !isnan(snap->temperature_c)) {
        printf(
            "{\"t\":1,\"temp_c\":%.2f,\"temp_v\":%.3f,\"temp_st\":\"%s\","
            "\"turb_raw\":%d,\"turb_v\":%.3f,\"turb_st\":\"%s\","
            "\"level_pct\":%d,\"level_raw\":%d,\"level_v\":%.3f,"
            "\"level_label\":\"%s\",\"level_st\":\"%s\","
            "\"connected\":%d,\"active\":%d}\n",
            snap->temperature_c,
            snap->temperature_v,
            state_str(snap->temperature),
            snap->turbidity_raw,
            snap->turbidity_sensor_v,
            state_str(snap->turbidity),
            snap->level.percent,
            snap->level.raw_on,
            snap->level.voltage,
            snap->level.level_label,
            state_str(snap->water_level),
            connected,
            active);
    } else {
        printf(
            "{\"t\":1,\"temp_c\":null,\"temp_v\":%.3f,\"temp_st\":\"%s\","
            "\"turb_raw\":%d,\"turb_v\":%.3f,\"turb_st\":\"%s\","
            "\"level_pct\":%d,\"level_raw\":%d,\"level_v\":%.3f,"
            "\"level_label\":\"%s\",\"level_st\":\"%s\","
            "\"connected\":%d,\"active\":%d}\n",
            snap->temperature_v,
            state_str(snap->temperature),
            snap->turbidity_raw,
            snap->turbidity_sensor_v,
            state_str(snap->turbidity),
            snap->level.percent,
            snap->level.raw_on,
            snap->level.voltage,
            snap->level.level_label,
            state_str(snap->water_level),
            connected,
            active);
    }

    fflush(stdout);
}
