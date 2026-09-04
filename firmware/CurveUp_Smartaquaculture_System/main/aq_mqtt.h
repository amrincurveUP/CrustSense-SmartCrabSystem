#pragma once

#include "esp_err.h"
#include <stdbool.h>

esp_err_t aq_mqtt_start(void);
void aq_mqtt_stop(void);
bool aq_mqtt_is_connected(void);
