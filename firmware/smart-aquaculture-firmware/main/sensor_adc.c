#include "sensor_adc.h"

#include "board_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static const char *TAG = "SENSOR_ADC";

static adc_oneshot_unit_handle_t s_adc_handle;
static adc_cali_handle_t s_cali_handle;
static bool s_cali_enabled;

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
    default: return ADC_CHANNEL_3;
    }
}

static esp_err_t init_calibration(void)
{
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .chan = ADC_CHANNEL_3,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (adc_cali_create_scheme_curve_fitting(&cali_config, &s_cali_handle) == ESP_OK) {
        s_cali_enabled = true;
        ESP_LOGI(TAG, "ADC calibration enabled (curve fitting)");
        return ESP_OK;
    }
#endif

    ESP_LOGW(TAG, "ADC calibration unavailable, using linear estimate");
    s_cali_enabled = false;
    return ESP_OK;
}

esp_err_t sensor_adc_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
    };

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };

    const int gpios[] = {
        PIN_TURBIDITY_ADC,
        PIN_TEMPERATURE_ADC,
        PIN_WATER_LEVEL_ADC,
    };

    for (size_t i = 0; i < sizeof(gpios) / sizeof(gpios[0]); i++) {
        ESP_ERROR_CHECK(adc_oneshot_config_channel(
            s_adc_handle,
            gpio_to_channel(gpios[i]),
            &chan_cfg));
    }

    return init_calibration();
}

esp_err_t sensor_adc_read_raw(int gpio, int *raw_out)
{
    if (raw_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    int total = 0;
    for (int i = 0; i < ADC_SAMPLE_COUNT; i++) {
        int sample = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(
            s_adc_handle,
            gpio_to_channel(gpio),
            &sample));
        total += sample;
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    *raw_out = total / ADC_SAMPLE_COUNT;
    return ESP_OK;
}

esp_err_t sensor_adc_read_voltage(int gpio, float *voltage_out)
{
    if (voltage_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    int raw = 0;
    ESP_ERROR_CHECK(sensor_adc_read_raw(gpio, &raw));
    return sensor_adc_raw_to_voltage(raw, voltage_out);
}

esp_err_t sensor_adc_raw_to_voltage(int raw, float *voltage_out)
{
    if (voltage_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_cali_enabled) {
        int mv = 0;
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(s_cali_handle, raw, &mv));
        *voltage_out = mv / 1000.0f;
        return ESP_OK;
    }

    *voltage_out = (raw / 4095.0f) * 3.3f;
    return ESP_OK;
}
