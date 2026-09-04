#pragma once

#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

esp_err_t sensor_adc_init(void);

/* Oversampled read — fills raw and/or volts (either may be NULL). */
esp_err_t sensor_adc_read(adc_channel_t channel, int *raw_out, float *volts_out);

esp_err_t sensor_adc_read_raw(adc_channel_t channel, int *raw_out);
esp_err_t sensor_adc_read_voltage(adc_channel_t channel, float *volts_out);
bool sensor_adc_ready(void);
