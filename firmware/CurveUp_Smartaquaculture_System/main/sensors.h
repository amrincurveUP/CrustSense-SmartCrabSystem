#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool enabled;
    bool connected;
    int raw;
    float volts;
    float celsius;   /* valid when temperature sensor */
    float percent;   /* valid when level sensor */
} sensor_reading_t;

typedef struct {
    sensor_reading_t temperature;
    sensor_reading_t turbidity;
    sensor_reading_t water_level;
} sensor_snapshot_t;

void sensors_init(void);
void sensors_read_all(sensor_snapshot_t *out);
