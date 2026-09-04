#include "pump.h"

#include "board_config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "PUMP";

static bool s_on;
static int64_t s_on_since_us;

/*
 * 5V active-LOW relay + 3.3V ESP:
 *   ON  = drive IN hard to GND (sink) → opto LED on → relay clicks ON
 *   OFF = release pin (Hi-Z / open-drain high) → module 5V pull-up turns LED fully off
 *
 * Driving OFF as GPIO=1 (3.3V) often FAILS: current still flows 5V→LED→3.3V and
 * the relay stays ON — exactly "Start works, Stop does not".
 */

static void gpio_force_on(void)
{
#if PUMP_ACTIVE_LOW
    gpio_set_direction(PUMP_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(PUMP_GPIO, 0);
#else
    gpio_set_direction(PUMP_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(PUMP_GPIO, 1);
#endif
}

static void gpio_force_off(void)
{
#if PUMP_ACTIVE_LOW
    /* Release line so relay module pull-up can go to 5V */
    gpio_set_level(PUMP_GPIO, 1);
    gpio_set_direction(PUMP_GPIO, GPIO_MODE_INPUT);
    gpio_pullup_dis(PUMP_GPIO);
    gpio_pulldown_dis(PUMP_GPIO);
#else
    gpio_set_direction(PUMP_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(PUMP_GPIO, 0);
#endif
}

esp_err_t pump_init(void)
{
    gpio_reset_pin(PUMP_GPIO);
    gpio_force_off();
    s_on = false;
    s_on_since_us = 0;
    ESP_LOGI(TAG, "GPIO%d ready (active_%s, Hi-Z off), default OFF",
             (int)PUMP_GPIO, PUMP_ACTIVE_LOW ? "LOW" : "HIGH");

#if PUMP_BOOT_SELFTEST
    ESP_LOGW(TAG, "=== RELAY SELF-TEST GPIO%d (listen for ON click then OFF click) ===",
             (int)PUMP_GPIO);
    for (int i = 0; i < 4; i++) {
        gpio_force_on();
        vTaskDelay(pdMS_TO_TICKS(500));
        gpio_force_off();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    gpio_force_off();
    s_on = false;
    ESP_LOGI(TAG, "Self-test done — OFF");
#endif

    return ESP_OK;
}

void pump_set(bool on)
{
    if (on) {
        gpio_force_on();
    } else {
        gpio_force_off();
    }
    s_on = on;
    s_on_since_us = on ? esp_timer_get_time() : 0;
    ESP_LOGI(TAG, "Pump %s (GPIO%d %s)",
             on ? "ON" : "OFF",
             (int)PUMP_GPIO,
             on ? "DRIVE_LOW" : "Hi-Z_OFF");
}

void pump_run_click_test(int seconds)
{
    if (seconds < 1) {
        seconds = 1;
    }
    if (seconds > 30) {
        seconds = 30;
    }
    ESP_LOGW(TAG, "Dashboard relay test: blink %d s on GPIO%d", seconds, (int)PUMP_GPIO);
    for (int i = 0; i < seconds; i++) {
        gpio_force_on();
        s_on = true;
        s_on_since_us = esp_timer_get_time();
        vTaskDelay(pdMS_TO_TICKS(500));
        gpio_force_off();
        s_on = false;
        s_on_since_us = 0;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    gpio_force_off();
    s_on = false;
    ESP_LOGI(TAG, "Relay test finished — OFF");
}

bool pump_is_on(void)
{
    return s_on;
}

uint32_t pump_run_ms(void)
{
    if (!s_on || s_on_since_us == 0) {
        return 0;
    }
    return (uint32_t)((esp_timer_get_time() - s_on_since_us) / 1000);
}
