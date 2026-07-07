#include "sensor_status.h"

#include <math.h>
#include <stdio.h>

#include "board_config.h"
#include "esp_log.h"

static const char *TAG = "SENSOR_STATUS";

#define TURBIDITY_HISTORY_LEN 8

static int s_turbidity_history[TURBIDITY_HISTORY_LEN];
static int s_turbidity_history_count;

static const char *connection_label(sensor_connection_t state)
{
    switch (state) {
    case SENSOR_ACTIVE:
        return "ACTIVE";
    case SENSOR_CONNECTED:
        return "CONNECTED (weak)";
    default:
        return "NOT CONNECTED";
    }
}

static int turbidity_history_range(void)
{
    if (s_turbidity_history_count < 2) {
        return 0;
    }

    int min_raw = 4095;
    int max_raw = 0;
    for (int i = 0; i < s_turbidity_history_count; i++) {
        if (s_turbidity_history[i] < min_raw) {
            min_raw = s_turbidity_history[i];
        }
        if (s_turbidity_history[i] > max_raw) {
            max_raw = s_turbidity_history[i];
        }
    }

    return max_raw - min_raw;
}

void sensor_status_init(void)
{
    s_turbidity_history_count = 0;
}

void sensor_status_update_turbidity_history(int raw)
{
    int index = s_turbidity_history_count % TURBIDITY_HISTORY_LEN;
    s_turbidity_history[index] = raw;
    if (s_turbidity_history_count < TURBIDITY_HISTORY_LEN) {
        s_turbidity_history_count++;
    }
}

void sensor_status_evaluate(sensor_snapshot_t *snap)
{
    if (snap == NULL) {
        return;
    }

    snap->turbidity_variation = turbidity_history_range();
    snap->turbidity_responding = snap->turbidity_variation >= 20;
    snap->temperature_valid = !isnan(snap->temperature_c);

    if (snap->turbidity_sensor_v >= 1.0f || snap->turbidity_responding) {
        snap->turbidity = SENSOR_ACTIVE;
    } else if (snap->turbidity_sensor_v >= 0.20f) {
        snap->turbidity = SENSOR_CONNECTED;
    } else {
        snap->turbidity = SENSOR_NOT_CONNECTED;
    }

    if (snap->temperature_v >= 0.55f && snap->temperature_v <= 2.95f &&
        snap->temperature_valid && snap->temperature_c >= 5.0f &&
        snap->temperature_c <= 45.0f) {
        snap->temperature = SENSOR_ACTIVE;
    } else if (snap->temperature_v >= 0.20f && snap->temperature_v < 3.2f) {
        snap->temperature = SENSOR_CONNECTED;
    } else {
        snap->temperature = SENSOR_NOT_CONNECTED;
    }

    if (snap->level_delta >= 40) {
        snap->water_level = SENSOR_ACTIVE;
    } else if (snap->level_delta >= 15 || snap->level_v >= 0.08f) {
        snap->water_level = SENSOR_CONNECTED;
    } else {
        snap->water_level = SENSOR_NOT_CONNECTED;
    }
}

int sensor_status_count_connected(const sensor_snapshot_t *snap)
{
    int count = 0;

    if (snap->turbidity != SENSOR_NOT_CONNECTED) {
        count++;
    }
    if (snap->temperature != SENSOR_NOT_CONNECTED) {
        count++;
    }
    if (snap->water_level != SENSOR_NOT_CONNECTED) {
        count++;
    }

    return count;
}

int sensor_status_count_active(const sensor_snapshot_t *snap)
{
    int count = 0;

    if (snap->turbidity == SENSOR_ACTIVE) {
        count++;
    }
    if (snap->temperature == SENSOR_ACTIVE) {
        count++;
    }
    if (snap->water_level == SENSOR_ACTIVE) {
        count++;
    }

    return count;
}

void sensor_status_print_dashboard(const sensor_snapshot_t *snap)
{
    const int connected = sensor_status_count_connected(snap);
    const int active = sensor_status_count_active(snap);

    ESP_LOGI(TAG, " ");
    ESP_LOGI(TAG, "========== SENSOR DASHBOARD ==========");
    ESP_LOGI(TAG, "Summary : %d of 3 connected | %d of 3 active", connected, active);
    ESP_LOGI(TAG, "--------------------------------------");

    ESP_LOGI(TAG, "[1] TURBIDITY  GPIO%-2d  -> %s",
             PIN_TURBIDITY_ADC,
             connection_label(snap->turbidity));
    ESP_LOGI(TAG, "    Value   : raw=%-4d  adc=%.3fV  sensor=%.3fV",
             snap->turbidity_raw,
             snap->turbidity_adc_v,
             snap->turbidity_sensor_v);
    if (snap->turbidity == SENSOR_NOT_CONNECTED) {
        ESP_LOGW(TAG, "    Hint    : Needs 5V + probe in water + divider to GPIO%d", PIN_TURBIDITY_ADC);
    } else if (!snap->turbidity_responding) {
        ESP_LOGW(TAG, "    Hint    : Signal stuck (change water/stir and watch variation=%d)",
                 snap->turbidity_variation);
    } else {
        ESP_LOGI(TAG, "    Hint    : Sensor is responding to water changes");
    }

    ESP_LOGI(TAG, "--------------------------------------");
    ESP_LOGI(TAG, "[2] TEMPERATURE GPIO%-2d -> %s",
             PIN_TEMPERATURE_ADC,
             connection_label(snap->temperature));
    if (snap->temperature_valid) {
        ESP_LOGI(TAG, "    Value   : %.2f C  (adc=%.3fV)", snap->temperature_c, snap->temperature_v);
    } else {
        ESP_LOGI(TAG, "    Value   : invalid  (adc=%.3fV)", snap->temperature_v);
    }
    if (snap->temperature == SENSOR_NOT_CONNECTED) {
        ESP_LOGW(TAG, "    Hint    : Expect ~1.5V at room temp. Check 3.3V->10k->GPIO%d->probe->GND",
                 PIN_TEMPERATURE_ADC);
    } else if (snap->temperature == SENSOR_CONNECTED) {
        ESP_LOGW(TAG, "    Hint    : Electrical signal seen, but reading not in valid range");
    } else {
        ESP_LOGI(TAG, "    Hint    : Temperature reading looks valid");
    }

    ESP_LOGI(TAG, "--------------------------------------");
    ESP_LOGI(TAG, "[3] WATER LEVEL GPIO%-2d -> %s",
             PIN_WATER_LEVEL_ADC,
             connection_label(snap->water_level));
    ESP_LOGI(TAG, "    Value   : raw=%-4d  voltage=%.3fV  power_delta=%d",
             snap->level_raw,
             snap->level_v,
             snap->level_delta);
    ESP_LOGI(TAG, "    Power   : GPIO%d drives sensor + pin", PIN_WATER_LEVEL_POWER);
    if (snap->water_level == SENSOR_NOT_CONNECTED) {
        ESP_LOGW(TAG, "    Hint    : Wire S->GPIO%d, +->GPIO%d, -->GND", PIN_WATER_LEVEL_ADC, PIN_WATER_LEVEL_POWER);
    } else if (snap->water_level == SENSOR_CONNECTED) {
        ESP_LOGW(TAG, "    Hint    : Sensor powered, but wetting change is weak");
    } else {
        ESP_LOGI(TAG, "    Hint    : Sensor responds when water touches the strip");
    }

    ESP_LOGI(TAG, "======================================");
}
