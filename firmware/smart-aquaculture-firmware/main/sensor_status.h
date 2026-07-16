#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "water_level.h"

typedef enum {
    SENSOR_NOT_CONNECTED = 0,
    SENSOR_CONNECTED,
    SENSOR_ACTIVE,
} sensor_state_t;

typedef struct {
    sensor_state_t turbidity;
    sensor_state_t temperature;
    sensor_state_t water_level;

    int turbidity_raw;
    float turbidity_adc_v;
    float turbidity_sensor_v;
    int turbidity_variation;
    bool turbidity_responding;

    float temperature_v;
    float temperature_c;
    bool temperature_valid;

    water_level_reading_t level;
} sensor_snapshot_t;

void sensor_status_init(void);
void sensor_status_note_turbidity(int raw);
void sensor_status_evaluate(sensor_snapshot_t *snap);
void sensor_status_print(const sensor_snapshot_t *snap);
