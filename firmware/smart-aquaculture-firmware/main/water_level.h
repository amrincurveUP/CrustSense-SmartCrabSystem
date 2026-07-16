#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int raw_off;
    int raw_on;
    int delta;
    float voltage;
    int percent;
    const char *level_label;
    bool wiring_suspect;
} water_level_reading_t;

void water_level_init(void);
void water_level_read(water_level_reading_t *out);
