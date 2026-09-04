#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

esp_err_t pump_init(void);
void pump_set(bool on);
bool pump_is_on(void);
uint32_t pump_run_ms(void);
/** Blink relay for hardware check (blocks caller briefly). */
void pump_run_click_test(int seconds);
