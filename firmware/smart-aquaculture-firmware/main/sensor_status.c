#include "sensor_status.h"

#include <math.h>

#include "board_config.h"
#include "esp_log.h"

static const char *TAG = "SENSOR_STATUS";

#define TURB_HISTORY_LEN 8

static int s_turb_hist[TURB_HISTORY_LEN];
static int s_turb_hist_n;

static const char *label(sensor_state_t s)
{
    switch (s) {
    case SENSOR_ACTIVE:     return "ACTIVE";
    case SENSOR_CONNECTED:  return "CONNECTED (weak)";
    default:                return "NOT CONNECTED";
    }
}

static int turbidity_range(void)
{
    if (s_turb_hist_n < 2) {
        return 0;
    }
    int lo = 4095, hi = 0;
    for (int i = 0; i < s_turb_hist_n; i++) {
        if (s_turb_hist[i] < lo) {
            lo = s_turb_hist[i];
        }
        if (s_turb_hist[i] > hi) {
            hi = s_turb_hist[i];
        }
    }
    return hi - lo;
}

void sensor_status_init(void)
{
    s_turb_hist_n = 0;
}

void sensor_status_note_turbidity(int raw)
{
    s_turb_hist[s_turb_hist_n % TURB_HISTORY_LEN] = raw;
    if (s_turb_hist_n < TURB_HISTORY_LEN) {
        s_turb_hist_n++;
    }
}

void sensor_status_evaluate(sensor_snapshot_t *snap)
{
    if (snap == NULL) {
        return;
    }

    snap->turbidity_variation = turbidity_range();
    snap->turbidity_responding = snap->turbidity_variation >= TURBIDITY_VARIATION_ACTIVE;
    snap->temperature_valid = !isnan(snap->temperature_c);

    if (snap->turbidity_sensor_v >= TURBIDITY_VOLT_ACTIVE || snap->turbidity_responding) {
        snap->turbidity = SENSOR_ACTIVE;
    } else if (snap->turbidity_sensor_v >= TURBIDITY_VOLT_CONNECTED) {
        snap->turbidity = SENSOR_CONNECTED;
    } else {
        snap->turbidity = SENSOR_NOT_CONNECTED;
    }

    if (snap->temperature_v >= TEMP_VOLT_ACTIVE_MIN &&
        snap->temperature_v <= TEMP_VOLT_ACTIVE_MAX &&
        snap->temperature_valid &&
        snap->temperature_c >= TEMP_C_ACTIVE_MIN &&
        snap->temperature_c <= TEMP_C_ACTIVE_MAX) {
        snap->temperature = SENSOR_ACTIVE;
    } else if (snap->temperature_v >= TEMP_VOLT_CONNECTED_MIN && snap->temperature_v < 3.2f) {
        snap->temperature = SENSOR_CONNECTED;
    } else {
        snap->temperature = SENSOR_NOT_CONNECTED;
    }

    if (snap->level.wiring_suspect) {
        snap->water_level = SENSOR_NOT_CONNECTED;
    } else if (snap->level.voltage >= LEVEL_VOLT_ACTIVE ||
               snap->level.delta >= LEVEL_DELTA_ACTIVE) {
        snap->water_level = SENSOR_ACTIVE;
    } else if (snap->level.voltage >= LEVEL_VOLT_CONNECTED ||
               snap->level.delta >= LEVEL_DELTA_CONNECTED) {
        snap->water_level = SENSOR_CONNECTED;
    } else {
        snap->water_level = SENSOR_NOT_CONNECTED;
    }
}

static int count_state(const sensor_snapshot_t *snap, sensor_state_t want)
{
    int n = 0;
    if (snap->turbidity == want) {
        n++;
    }
    if (snap->temperature == want) {
        n++;
    }
    if (snap->water_level == want) {
        n++;
    }
    return n;
}

void sensor_status_print(const sensor_snapshot_t *snap)
{
    const int connected =
        (snap->turbidity != SENSOR_NOT_CONNECTED ? 1 : 0) +
        (snap->temperature != SENSOR_NOT_CONNECTED ? 1 : 0) +
        (snap->water_level != SENSOR_NOT_CONNECTED ? 1 : 0);
    const int active = count_state(snap, SENSOR_ACTIVE);

    ESP_LOGI(TAG, " ");
    ESP_LOGI(TAG, "========== SENSOR DASHBOARD ==========");
    ESP_LOGI(TAG, "Summary : %d of 3 connected | %d of 3 active", connected, active);
    ESP_LOGI(TAG, "--------------------------------------");

    ESP_LOGI(TAG, "[1] TURBIDITY   GPIO%-2d -> %s", PIN_TURBIDITY_ADC, label(snap->turbidity));
    ESP_LOGI(TAG, "    raw=%-4d  adc=%.3fV  sensor=%.3fV  variation=%d",
             snap->turbidity_raw, snap->turbidity_adc_v,
             snap->turbidity_sensor_v, snap->turbidity_variation);
    if (snap->turbidity == SENSOR_NOT_CONNECTED) {
        ESP_LOGW(TAG, "    -> Needs 5V, probe in water, divider to GPIO%d", PIN_TURBIDITY_ADC);
    }

    ESP_LOGI(TAG, "--------------------------------------");
    ESP_LOGI(TAG, "[2] TEMPERATURE GPIO%-2d -> %s", PIN_TEMPERATURE_ADC, label(snap->temperature));
    if (snap->temperature_valid) {
        ESP_LOGI(TAG, "    %.2f C  (adc=%.3fV)", snap->temperature_c, snap->temperature_v);
    } else {
        ESP_LOGI(TAG, "    invalid  (adc=%.3fV)", snap->temperature_v);
    }
    if (snap->temperature == SENSOR_NOT_CONNECTED) {
        ESP_LOGW(TAG, "    -> Unplugged pin floats. Wired: 3.3V->10k->GPIO%d->probe->GND (~1.5V)",
                 PIN_TEMPERATURE_ADC);
    } else if (snap->temperature == SENSOR_CONNECTED) {
        ESP_LOGW(TAG, "    -> Signal present but out of valid range");
    }

    ESP_LOGI(TAG, "--------------------------------------");
    ESP_LOGI(TAG, "[3] WATER LEVEL HW-038  GPIO%-2d -> %s",
             PIN_WATER_LEVEL_SIGNAL, label(snap->water_level));
    ESP_LOGI(TAG, "    level=%s  (%d%% wet)  raw=%d  V=%.3fV  delta=%d",
             snap->level.level_label,
             snap->level.percent,
             snap->level.raw_on,
             snap->level.voltage,
             snap->level.delta);
#if WATER_LEVEL_POWER_FROM_GPIO
    ESP_LOGI(TAG, "    wiring: S->GPIO%d  +->GPIO%d  - ->GND", PIN_WATER_LEVEL_SIGNAL, PIN_WATER_LEVEL_POWER);
#else
    ESP_LOGI(TAG, "    wiring: S->GPIO%d  +->3.3V  - ->GND", PIN_WATER_LEVEL_SIGNAL);
#endif
    if (snap->level.wiring_suspect) {
        ESP_LOGW(TAG, "    -> Negative delta: swap S and + wires");
    } else if (snap->water_level == SENSOR_NOT_CONNECTED) {
        ESP_LOGW(TAG, "    -> Dip HW-038 strip in water; add/remove water to see %% change");
    } else if (snap->water_level == SENSOR_CONNECTED) {
        ESP_LOGW(TAG, "    -> Weak signal — lower strip deeper into the cup");
    } else {
        ESP_LOGI(TAG, "    -> Sensor responding to water level changes");
    }

    ESP_LOGI(TAG, "======================================");
}
