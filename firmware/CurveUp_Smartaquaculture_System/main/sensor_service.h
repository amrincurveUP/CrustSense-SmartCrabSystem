#pragma once

#include "esp_err.h"
#include "sensors.h"
#include <stddef.h>

esp_err_t sensor_service_init(void);
void sensor_service_get_snapshot(sensor_snapshot_t *out);

/* Dashboard / MQTT tanks JSON (same schema as old firmware) */
size_t sensor_service_json(char *buf, size_t len);
