#pragma once

#include <stddef.h>

#include "esp_err.h"
#include "sensor_status.h"

esp_err_t sensor_service_init(void);
void sensor_service_get_snapshot(sensor_snapshot_t *out);
size_t sensor_service_json(char *buf, size_t len);
