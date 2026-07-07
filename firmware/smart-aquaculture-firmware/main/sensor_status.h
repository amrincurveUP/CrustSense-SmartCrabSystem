#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SENSOR_NOT_CONNECTED = 0,
    SENSOR_CONNECTED,
    SENSOR_ACTIVE,
} sensor_connection_t;

typedef struct {
    sensor_connection_t turbidity;
    sensor_connection_t temperature;
    sensor_connection_t water_level;

    int turbidity_raw;
    float turbidity_adc_v;
    float turbidity_sensor_v;
    float temperature_v;
    float temperature_c;
    int level_raw;
    float level_v;
    int level_delta;

    int turbidity_variation;
    bool temperature_valid;
    bool turbidity_responding;
} sensor_snapshot_t;

void sensor_status_init(void);
void sensor_status_update_turbidity_history(int raw);
void sensor_status_evaluate(sensor_snapshot_t *snap);
void sensor_status_print_dashboard(const sensor_snapshot_t *snap);

int sensor_status_count_connected(const sensor_snapshot_t *snap);
int sensor_status_count_active(const sensor_snapshot_t *snap);
