#include "sensors.h"

#include "board_config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "math.h"
#include "sensor_adc.h"
#include <string.h>

static const char *TAG = "SENSORS";

static float s_temp_ema = NAN;
static float s_temp_v_ema = NAN;
static bool s_temp_filter_ready;

static float clampf(float v, float lo, float hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static float ntc_celsius_from_volts(float v)
{
    if (v < 0.05f || v >= 3.25f) {
        return NAN;
    }

#if NTC_DIVIDER_NTC_TO_GND
    float r_ntc = NTC_R_SERIES_OHMS * v / (3.3f - v);
#else
    float r_ntc = NTC_R_SERIES_OHMS * (3.3f - v) / v;
#endif

    float inv_t = (1.0f / NTC_T0_KELVIN) + (1.0f / NTC_BETA) * logf(r_ntc / NTC_R0_OHMS);
    return (1.0f / inv_t) - 273.15f;
}

static float stabilize_temp_volts(float v)
{
    if (!s_temp_filter_ready || isnan(s_temp_v_ema)) {
        s_temp_v_ema = v;
        return v;
    }

    float delta = fabsf(v - s_temp_v_ema);
    if (delta > TEMP_MAX_STEP_V) {
        ESP_LOGW(TAG, "Temp ADC spike ignored: %.3f V (was %.3f V)", v, s_temp_v_ema);
        return s_temp_v_ema;
    }

    s_temp_v_ema = (TEMP_EMA_ALPHA * v) + ((1.0f - TEMP_EMA_ALPHA) * s_temp_v_ema);
    return s_temp_v_ema;
}

static float apply_temp_ema(float celsius)
{
    if (isnan(celsius)) {
        return celsius;
    }
    if (!s_temp_filter_ready || isnan(s_temp_ema)) {
        s_temp_ema = celsius;
        s_temp_filter_ready = true;
        return s_temp_ema;
    }
    s_temp_ema = (TEMP_EMA_ALPHA * celsius) + ((1.0f - TEMP_EMA_ALPHA) * s_temp_ema);
    return s_temp_ema;
}

static float level_percent_from_volts(float v)
{
    /* Piecewise bands from board_config calibration */
    if (v <= CAL_LEVEL_V_DRY + 0.001f || v < CAL_LEVEL_V_LOW_START) {
        return 0.0f; /* DRY */
    }

    if (v <= CAL_LEVEL_V_LOW_END) {
        /* 0.500 V → 1%, 1.300 V → 30% */
        float t = (v - CAL_LEVEL_V_LOW_START) /
                  (CAL_LEVEL_V_LOW_END - CAL_LEVEL_V_LOW_START);
        return 1.0f + t * (30.0f - 1.0f);
    }

    if (v < CAL_LEVEL_V_MED_START) {
        /* Gap 1.300–1.400 V: hold at 30% */
        return 30.0f;
    }

    if (v <= CAL_LEVEL_V_MED_END) {
        /* 1.400 V → 30%, 1.500 V → 70% */
        float t = (v - CAL_LEVEL_V_MED_START) /
                  (CAL_LEVEL_V_MED_END - CAL_LEVEL_V_MED_START);
        return 30.0f + t * (70.0f - 30.0f);
    }

    if (v < CAL_LEVEL_V_FULL_START) {
        /* Gap 1.500–1.600 V: hold at 70% */
        return 70.0f;
    }

    if (v <= CAL_LEVEL_V_FULL_END) {
        /* 1.600 V → 70%, 1.800 V → 100% */
        float t = (v - CAL_LEVEL_V_FULL_START) /
                  (CAL_LEVEL_V_FULL_END - CAL_LEVEL_V_FULL_START);
        return 70.0f + t * (100.0f - 70.0f);
    }

    /* >1.800 V → OVER (>100%) */
    float span = CAL_LEVEL_V_FULL_END - CAL_LEVEL_V_FULL_START;
    float t = (v - CAL_LEVEL_V_FULL_START) / (span > 0.05f ? span : 0.1f);
    float pct = 70.0f + t * (100.0f - 70.0f);
    return clampf(pct, 0.0f, 150.0f);
}

static void read_channel(adc_channel_t ch, sensor_reading_t *r, bool as_temp, bool as_level)
{
    memset(r, 0, sizeof(*r));
    r->enabled = true;
    r->celsius = NAN;

    if (sensor_adc_read(ch, &r->raw, &r->volts) != ESP_OK) {
        return;
    }

    if (as_temp) {
        float stable_v = stabilize_temp_volts(r->volts);
        r->volts = stable_v;
        r->connected = (stable_v >= SENSOR_CONNECTED_MIN_V);
        if (r->connected) {
            float raw_c = ntc_celsius_from_volts(stable_v);
            if (!isnan(raw_c)) {
                raw_c += CAL_TEMP_OFFSET_C;
            }
            r->celsius = apply_temp_ema(raw_c);
        } else {
            ESP_LOGW(TAG, "Temp looks disconnected (%.3f V)", r->volts);
        }
        return;
    }

    if (as_level) {
        /* Dry probes often sit near 0 V — that is a valid reading, not "OFF". */
        r->connected = true;
        r->percent = level_percent_from_volts(r->volts);
        return;
    }

    r->connected = (r->volts >= SENSOR_CONNECTED_MIN_V);
}

void sensors_init(void)
{
    ESP_ERROR_CHECK(sensor_adc_init());
    s_temp_ema = NAN;
    s_temp_v_ema = NAN;
    s_temp_filter_ready = false;

#if SENSOR_WATER_LEVEL_ENABLED && WATER_LEVEL_POWER_FROM_GPIO
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << WATER_LEVEL_POWER_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);
    gpio_set_level(WATER_LEVEL_POWER_GPIO, 1);
#endif

    ESP_LOGI(TAG, "Sensors init — cal temp_off=%.2f level bands dry/low/med/full turb_clear=%.2fV",
             CAL_TEMP_OFFSET_C, CAL_TURB_V_CLEAR);
}

void sensors_read_all(sensor_snapshot_t *out)
{
    memset(out, 0, sizeof(*out));

#if SENSOR_TEMP_ENABLED
    read_channel(TEMP_ADC_CHANNEL, &out->temperature, true, false);
#else
    out->temperature.enabled = false;
#endif

#if SENSOR_TURBIDITY_ENABLED
    read_channel(TURBIDITY_ADC_CHANNEL, &out->turbidity, false, false);
#else
    out->turbidity.enabled = false;
#endif

#if SENSOR_WATER_LEVEL_ENABLED
    read_channel(WATER_LEVEL_ADC_CHANNEL, &out->water_level, false, true);
#else
    out->water_level.enabled = false;
#endif
}
