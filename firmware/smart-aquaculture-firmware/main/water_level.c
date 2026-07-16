#include "water_level.h"

#include "board_config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sensor_adc.h"

static const char *TAG = "WATER_LEVEL";

static const char *level_label_from_percent(int percent)
{
    if (percent <= 5) {
        return "DRY";
    }
    if (percent <= 25) {
        return "LOW";
    }
    if (percent <= 60) {
        return "MEDIUM";
    }
    if (percent <= 85) {
        return "HIGH";
    }
    return "FULL";
}

static int raw_to_percent(int raw_on)
{
    if (raw_on <= 0) {
        return 0;
    }

    int percent = (raw_on * 100) / LEVEL_RAW_FULL_SCALE;
    if (percent > 100) {
        percent = 100;
    }
    return percent;
}

void water_level_init(void)
{
#if WATER_LEVEL_POWER_FROM_GPIO
    gpio_config_t pwr = {
        .pin_bit_mask = 1ULL << PIN_WATER_LEVEL_POWER,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&pwr);
    gpio_set_level(PIN_WATER_LEVEL_POWER, 0);
    ESP_LOGI(TAG, "HW-038 init: signal=GPIO%d power=GPIO%d (pulsed)", PIN_WATER_LEVEL_SIGNAL, PIN_WATER_LEVEL_POWER);
#else
    ESP_LOGI(TAG, "HW-038 init: signal=GPIO%d power=3.3V constant", PIN_WATER_LEVEL_SIGNAL);
#endif
}

static int read_with_power(bool on)
{
#if WATER_LEVEL_POWER_FROM_GPIO
    gpio_set_level(PIN_WATER_LEVEL_POWER, on ? 1 : 0);
    vTaskDelay(pdMS_TO_TICKS(on ? WATER_LEVEL_POWER_MS : 5));
#else
    (void)on;
    vTaskDelay(pdMS_TO_TICKS(WATER_LEVEL_POWER_MS));
#endif

    int raw = 0;
    ESP_ERROR_CHECK(sensor_adc_read_raw(PIN_WATER_LEVEL_SIGNAL, &raw));
    return raw;
}

void water_level_read(water_level_reading_t *out)
{
    if (out == NULL) {
        return;
    }

#if WATER_LEVEL_POWER_FROM_GPIO
    out->raw_off = read_with_power(false);
    out->raw_on = read_with_power(true);
    gpio_set_level(PIN_WATER_LEVEL_POWER, 0);
    out->delta = out->raw_on - out->raw_off;
    out->wiring_suspect = out->delta < LEVEL_DELTA_SWAP_FAULT;
#else
    out->raw_off = 0;
    out->raw_on = read_with_power(true);
    out->delta = out->raw_on;
    out->wiring_suspect = false;
#endif

    ESP_ERROR_CHECK(sensor_adc_raw_to_voltage(PIN_WATER_LEVEL_SIGNAL, out->raw_on, &out->voltage));
    out->percent = raw_to_percent(out->raw_on);
    out->level_label = level_label_from_percent(out->percent);
}
