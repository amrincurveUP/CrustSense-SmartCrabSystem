#include "sensor_adc.h"

#include "board_config.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include <limits.h>
#include <string.h>

static const char *TAG = "ADC";

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali;
static bool s_ready;

static esp_err_t configure_channel(adc_channel_t ch)
{
    adc_oneshot_chan_cfg_t cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    return adc_oneshot_config_channel(s_adc, ch, &cfg);
}

esp_err_t sensor_adc_init(void)
{
    if (s_ready) {
        return ESP_OK;
    }

    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = SENSOR_ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc));

#if SENSOR_TEMP_ENABLED
    ESP_ERROR_CHECK(configure_channel(TEMP_ADC_CHANNEL));
#endif
#if SENSOR_TURBIDITY_ENABLED
    ESP_ERROR_CHECK(configure_channel(TURBIDITY_ADC_CHANNEL));
#endif
#if SENSOR_WATER_LEVEL_ENABLED
    ESP_ERROR_CHECK(configure_channel(WATER_LEVEL_ADC_CHANNEL));
#endif

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = SENSOR_ADC_UNIT,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali) != ESP_OK) {
        ESP_LOGW(TAG, "ADC calibration unavailable — using raw estimate");
        s_cali = NULL;
    }
#else
    s_cali = NULL;
#endif

    s_ready = true;
    ESP_LOGI(TAG, "ADC oneshot ready (%s), oversample=%d", BOARD_NAME, ADC_OVERSAMPLE_COUNT);
    return ESP_OK;
}

bool sensor_adc_ready(void)
{
    return s_ready;
}

esp_err_t sensor_adc_read(adc_channel_t channel, int *raw_out, float *volts_out)
{
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }

    const int n = ADC_OVERSAMPLE_COUNT;
    if (n < 1) {
        return ESP_ERR_INVALID_ARG;
    }

    int64_t sum = 0;
    int min_s = INT_MAX;
    int max_s = INT_MIN;
    int got = 0;

    for (int i = 0; i < n; i++) {
        int sample = 0;
        esp_err_t err = adc_oneshot_read(s_adc, channel, &sample);
        if (err != ESP_OK) {
            return err;
        }
        sum += sample;
        if (sample < min_s) {
            min_s = sample;
        }
        if (sample > max_s) {
            max_s = sample;
        }
        got++;
        /* Small settle delay between samples (reduces WiFi/ADC crosstalk noise) */
        esp_rom_delay_us(ADC_SAMPLE_GAP_US);
    }

    int raw;
    if (got >= 4) {
        /* Drop outliers (min + max) then average */
        raw = (int)((sum - min_s - max_s) / (got - 2));
    } else {
        raw = (int)(sum / got);
    }

    if (raw_out != NULL) {
        *raw_out = raw;
    }

    if (volts_out != NULL) {
        int mv = 0;
        if (s_cali != NULL) {
            esp_err_t err = adc_cali_raw_to_voltage(s_cali, raw, &mv);
            if (err != ESP_OK) {
                return err;
            }
        } else {
            mv = (raw * ADC_VREF_MV) / 4095;
        }
        *volts_out = mv / 1000.0f;
    }

    return ESP_OK;
}

esp_err_t sensor_adc_read_raw(adc_channel_t channel, int *raw_out)
{
    return sensor_adc_read(channel, raw_out, NULL);
}

esp_err_t sensor_adc_read_voltage(adc_channel_t channel, float *volts_out)
{
    return sensor_adc_read(channel, NULL, volts_out);
}
