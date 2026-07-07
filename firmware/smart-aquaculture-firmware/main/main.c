#include <math.h>
#include <string.h>

#include "board_config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ntc_temp.h"
#include "sensor_adc.h"
#include "sensor_status.h"

static const char *TAG = "AQUA_MONITOR";

static void water_level_gpio_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << PIN_WATER_LEVEL_POWER,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(PIN_WATER_LEVEL_POWER, 0);
}

static int read_water_level_raw_with_power(bool power_on)
{
    gpio_set_level(PIN_WATER_LEVEL_POWER, power_on ? 1 : 0);
    vTaskDelay(pdMS_TO_TICKS(power_on ? WATER_LEVEL_POWER_ON_MS : 5));

    int raw = 0;
    ESP_ERROR_CHECK(sensor_adc_read_raw(PIN_WATER_LEVEL_ADC, &raw));
    return raw;
}

static float turbidity_sensor_voltage(float adc_voltage)
{
    return adc_voltage *
           (TURBIDITY_DIVIDER_R_TOP_OHMS + TURBIDITY_DIVIDER_R_BOTTOM_OHMS) /
           TURBIDITY_DIVIDER_R_BOTTOM_OHMS;
}

static void collect_snapshot(sensor_snapshot_t *snap)
{
    memset(snap, 0, sizeof(*snap));

    ESP_ERROR_CHECK(sensor_adc_read_raw(PIN_TURBIDITY_ADC, &snap->turbidity_raw));
    ESP_ERROR_CHECK(sensor_adc_read_voltage(PIN_TURBIDITY_ADC, &snap->turbidity_adc_v));
    snap->turbidity_sensor_v = turbidity_sensor_voltage(snap->turbidity_adc_v);
    sensor_status_update_turbidity_history(snap->turbidity_raw);

    ESP_ERROR_CHECK(sensor_adc_read_voltage(PIN_TEMPERATURE_ADC, &snap->temperature_v));
    snap->temperature_c = ntc_voltage_to_celsius(snap->temperature_v);

    const int level_off = read_water_level_raw_with_power(false);
    snap->level_raw = read_water_level_raw_with_power(true);
    gpio_set_level(PIN_WATER_LEVEL_POWER, 0);
    snap->level_delta = snap->level_raw - level_off;

    ESP_ERROR_CHECK(sensor_adc_raw_to_voltage(snap->level_raw, &snap->level_v));
}

void app_main(void)
{
    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, " Smart Aquaculture Sensor Test ");
    ESP_LOGI(TAG, " Cup / bench bring-up mode ");
    ESP_LOGI(TAG, "=================================");

    ESP_ERROR_CHECK(sensor_adc_init());
    water_level_gpio_init();
    sensor_status_init();

    ESP_LOGI(TAG, "Pin map:");
    ESP_LOGI(TAG, "  [1] Turbidity  : GPIO%d (ADC)", PIN_TURBIDITY_ADC);
    ESP_LOGI(TAG, "  [2] Temperature: GPIO%d (ADC)", PIN_TEMPERATURE_ADC);
    ESP_LOGI(TAG, "  [3] Water level: GPIO%d (signal), GPIO%d (power)", PIN_WATER_LEVEL_ADC, PIN_WATER_LEVEL_POWER);
    ESP_LOGI(TAG, "Status legend: ACTIVE = working | CONNECTED = weak | NOT CONNECTED = check wiring");

    while (true) {
        sensor_snapshot_t snap;
        collect_snapshot(&snap);
        sensor_status_evaluate(&snap);
        sensor_status_print_dashboard(&snap);

        vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
    }
}
