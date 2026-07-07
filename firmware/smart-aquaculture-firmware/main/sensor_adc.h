#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t sensor_adc_init(void);
esp_err_t sensor_adc_read_raw(int gpio, int *raw_out);
esp_err_t sensor_adc_read_voltage(int gpio, float *voltage_out);
esp_err_t sensor_adc_raw_to_voltage(int raw, float *voltage_out);
