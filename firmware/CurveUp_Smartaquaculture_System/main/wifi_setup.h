#pragma once

#include "esp_err.h"

esp_err_t wifi_setup_init(void);
const char *wifi_setup_get_ip(void);
