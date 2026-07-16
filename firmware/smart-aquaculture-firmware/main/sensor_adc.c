#include "sensor_adc.h"

#include "board_config.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SENSOR_ADC";

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali_temp;
static adc_cali_handle_t s_cali_turb;
static adc_cali_handle_t s_cali_level;
static bool s_cali_temp_ok;
static bool s_cali_turb_ok;
static bool s_cali_level_ok;

static adc_channel_t gpio_to_channel(int gpio)
{
    switch (gpio) {
    case 1:  return ADC_CHANNEL_0;
    case 2:  return ADC_CHANNEL_1;
    case 3:  return ADC_CHANNEL_2;
    case 4:  return ADC_CHANNEL_3;
    case 5:  return ADC_CHANNEL_4;
    case 6:  return ADC_CHANNEL_5;
    case 7:  return ADC_CHANNEL_6;
    case 8:  return ADC_CHANNEL_7;
    case 9:  return ADC_CHANNEL_8;
    case 10: return ADC_CHANNEL_9;
    default: return ADC_CHANNEL_4;
    }
}

static bool init_channel_calibration(int gpio, adc_cali_handle_t *handle)
{
    adc_cali_curve_fitting_config_t cfg = {
        .unit_id = ADC_UNIT_1,
        .chan = gpio_to_channel(gpio),
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (adc_cali_create_scheme_curve_fitting(&cfg, handle) == ESP_OK) {
        return true;
    }
#endif

    return false;
}

esp_err_t sensor_adc_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };

    const int pins[] = {
        PIN_TEMPERATURE_ADC,
        PIN_TURBIDITY_ADC,
        PIN_WATER_LEVEL_SIGNAL,
    };
    for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
        ESP_ERROR_CHECK(adc_oneshot_config_channel(
            s_adc, gpio_to_channel(pins[i]), &chan_cfg));
    }

    s_cali_temp_ok = init_channel_calibration(PIN_TEMPERATURE_ADC, &s_cali_temp);
    s_cali_turb_ok = init_channel_calibration(PIN_TURBIDITY_ADC, &s_cali_turb);
    s_cali_level_ok = init_channel_calibration(PIN_WATER_LEVEL_SIGNAL, &s_cali_level);

    ESP_LOGI(TAG, "ADC init: temp GPIO%d, turbidity GPIO%d, level GPIO%d",
             PIN_TEMPERATURE_ADC, PIN_TURBIDITY_ADC, PIN_WATER_LEVEL_SIGNAL);

    return ESP_OK;
}

esp_err_t sensor_adc_read_raw(int gpio, int *raw_out)
{
    if (raw_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    int total = 0;
    for (int i = 0; i < ADC_SAMPLE_COUNT; i++) {
        int sample = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(s_adc, gpio_to_channel(gpio), &sample));
        total += sample;
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    *raw_out = total / ADC_SAMPLE_COUNT;
    return ESP_OK;
}

esp_err_t sensor_adc_raw_to_voltage(int gpio, int raw, float *voltage_out)
{
    if (voltage_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    adc_cali_handle_t cali = NULL;
    bool cal_ok = false;

    if (gpio == PIN_TEMPERATURE_ADC && s_cali_temp_ok) {
        cali = s_cali_temp;
        cal_ok = true;
    } else if (gpio == PIN_TURBIDITY_ADC && s_cali_turb_ok) {
        cali = s_cali_turb;
        cal_ok = true;
    } else if (gpio == PIN_WATER_LEVEL_SIGNAL && s_cali_level_ok) {
        cali = s_cali_level;
        cal_ok = true;
    }

    if (cal_ok) {
        int mv = 0;
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali, raw, &mv));
        *voltage_out = mv / 1000.0f;
        return ESP_OK;
    }

    *voltage_out = (raw / 4095.0f) * 3.3f;
    return ESP_OK;
}

esp_err_t sensor_adc_read_voltage(int gpio, float *voltage_out)
{
    int raw = 0;
    ESP_ERROR_CHECK(sensor_adc_read_raw(gpio, &raw));
    return sensor_adc_raw_to_voltage(gpio, raw, voltage_out);
}
