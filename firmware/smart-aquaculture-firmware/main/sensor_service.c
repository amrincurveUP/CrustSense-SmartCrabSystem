#include "sensor_service.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "board_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "ntc_temp.h"
#include "sensor_adc.h"
#include "sensor_status.h"
#include "telemetry.h"
#include "thresholds.h"
#include "water_level.h"

static const char *TAG = "SENSOR_SVC";

static SemaphoreHandle_t s_lock;
static sensor_snapshot_t s_snap;

typedef struct {
    const char *id;
    const char *label;
    const char *species;
    bool live;
} tank_meta_t;

static const tank_meta_t TANKS[] = {
    { "L1", "L1 · Electric Blue", "Procambarus alleni", true },
    { "L2", "L2 · Red Claw",       "Procambarus clarkii", false },
    { "L3", "L3 · Electric Blue", "Procambarus alleni", false },
};

static void collect(sensor_snapshot_t *snap)
{
    memset(snap, 0, sizeof(*snap));

    ESP_ERROR_CHECK(sensor_adc_read_raw(PIN_TURBIDITY_ADC, &snap->turbidity_raw));
    ESP_ERROR_CHECK(sensor_adc_read_voltage(PIN_TURBIDITY_ADC, &snap->turbidity_adc_v));
    snap->turbidity_sensor_v = snap->turbidity_adc_v;
    sensor_status_note_turbidity(snap->turbidity_raw);

    ESP_ERROR_CHECK(sensor_adc_read_voltage(PIN_TEMPERATURE_ADC, &snap->temperature_v));
    snap->temperature_c = ntc_voltage_to_celsius(snap->temperature_v);

    water_level_read(&snap->level);
    sensor_status_evaluate(snap);
}

static void append_offline_tank(char *buf, size_t len, size_t *pos, const tank_meta_t *tank, bool first)
{
    *pos += (size_t)snprintf(
        buf + *pos,
        len - *pos,
        "%s{\"id\":\"%s\",\"label\":\"%s\",\"species\":\"%s\",\"live\":false,"
        "\"temperature\":{\"c\":null,\"v\":0,\"state\":\"OFF\",\"level\":\"NO DATA\",\"color\":\"#64748b\"},"
        "\"turbidity\":{\"raw\":0,\"v\":0,\"state\":\"OFF\",\"level\":\"NO DATA\",\"color\":\"#64748b\"},"
        "\"water_level\":{\"pct\":0,\"raw\":0,\"v\":0,\"label\":\"DRY\",\"state\":\"OFF\",\"level\":\"NO DATA\",\"color\":\"#64748b\"}}",
        first ? "" : ",",
        tank->id,
        tank->label,
        tank->species);
}

static void append_live_tank(char *buf, size_t len, size_t *pos, const tank_meta_t *tank,
                             const sensor_snapshot_t *snap, bool first)
{
    const bool temp_ok = snap->temperature_valid && !isnan(snap->temperature_c);
    const param_band_t temp_band = thresholds_temp_level(snap->temperature_c, temp_ok);
    const param_band_t turb_band = thresholds_turbidity_level(snap->turbidity_sensor_v);
    const param_band_t level_band = thresholds_water_level(snap->level.percent);

    if (temp_ok) {
        *pos += (size_t)snprintf(
            buf + *pos,
            len - *pos,
            "%s{\"id\":\"%s\",\"label\":\"%s\",\"species\":\"%s\",\"live\":true,"
            "\"temperature\":{\"c\":%.2f,\"v\":%.3f,\"state\":\"%s\",\"level\":\"%s\",\"color\":\"%s\"},"
            "\"turbidity\":{\"raw\":%d,\"v\":%.3f,\"state\":\"%s\",\"level\":\"%s\",\"color\":\"%s\"},"
            "\"water_level\":{\"pct\":%d,\"raw\":%d,\"v\":%.3f,\"label\":\"%s\",\"state\":\"%s\",\"level\":\"%s\",\"color\":\"%s\"}}",
            first ? "" : ",",
            tank->id,
            tank->label,
            tank->species,
            snap->temperature_c,
            snap->temperature_v,
            sensor_state_str(snap->temperature),
            temp_band.label,
            temp_band.color,
            snap->turbidity_raw,
            snap->turbidity_sensor_v,
            sensor_state_str(snap->turbidity),
            turb_band.label,
            turb_band.color,
            snap->level.percent,
            snap->level.raw_on,
            snap->level.voltage,
            snap->level.level_label,
            sensor_state_str(snap->water_level),
            level_band.label,
            level_band.color);
    } else {
        *pos += (size_t)snprintf(
            buf + *pos,
            len - *pos,
            "%s{\"id\":\"%s\",\"label\":\"%s\",\"species\":\"%s\",\"live\":true,"
            "\"temperature\":{\"c\":null,\"v\":%.3f,\"state\":\"%s\",\"level\":\"%s\",\"color\":\"%s\"},"
            "\"turbidity\":{\"raw\":%d,\"v\":%.3f,\"state\":\"%s\",\"level\":\"%s\",\"color\":\"%s\"},"
            "\"water_level\":{\"pct\":%d,\"raw\":%d,\"v\":%.3f,\"label\":\"%s\",\"state\":\"%s\",\"level\":\"%s\",\"color\":\"%s\"}}",
            first ? "" : ",",
            tank->id,
            tank->label,
            tank->species,
            snap->temperature_v,
            sensor_state_str(snap->temperature),
            temp_band.label,
            temp_band.color,
            snap->turbidity_raw,
            snap->turbidity_sensor_v,
            sensor_state_str(snap->turbidity),
            turb_band.label,
            turb_band.color,
            snap->level.percent,
            snap->level.raw_on,
            snap->level.voltage,
            snap->level.level_label,
            sensor_state_str(snap->water_level),
            level_band.label,
            level_band.color);
    }
}

static void sensor_task(void *arg)
{
    (void)arg;

    while (true) {
        sensor_snapshot_t local;
        collect(&local);

        if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
            s_snap = local;
            xSemaphoreGive(s_lock);
        }

        sensor_status_print(&local);
        telemetry_print_json(&local);

        vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
    }
}

esp_err_t sensor_service_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(sensor_adc_init());
    water_level_init();
    sensor_status_init();

    collect(&s_snap);

    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "L1 sensors GPIO%d(temp) GPIO%d(turb) GPIO%d(level)",
             PIN_TEMPERATURE_ADC, PIN_TURBIDITY_ADC, PIN_WATER_LEVEL_SIGNAL);
    return ESP_OK;
}

void sensor_service_get_snapshot(sensor_snapshot_t *out)
{
    if (out == NULL) {
        return;
    }

    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(200)) == pdTRUE) {
        *out = s_snap;
        xSemaphoreGive(s_lock);
    } else {
        *out = s_snap;
    }
}

size_t sensor_service_json(char *buf, size_t len)
{
    sensor_snapshot_t snap;
    sensor_service_get_snapshot(&snap);

    int connected = 0;
    int active = 0;

    if (snap.turbidity != SENSOR_NOT_CONNECTED) {
        connected++;
    }
    if (snap.temperature != SENSOR_NOT_CONNECTED) {
        connected++;
    }
    if (snap.water_level != SENSOR_NOT_CONNECTED) {
        connected++;
    }
    if (snap.turbidity == SENSOR_ACTIVE) {
        active++;
    }
    if (snap.temperature == SENSOR_ACTIVE) {
        active++;
    }
    if (snap.water_level == SENSOR_ACTIVE) {
        active++;
    }

    size_t pos = (size_t)snprintf(
        buf,
        len,
        "{\"connected\":%d,\"active\":%d,\"tanks\":[",
        connected,
        active);

    for (size_t i = 0; i < sizeof(TANKS) / sizeof(TANKS[0]); i++) {
        if (TANKS[i].live) {
            append_live_tank(buf, len, &pos, &TANKS[i], &snap, i == 0);
        } else {
            append_offline_tank(buf, len, &pos, &TANKS[i], i == 0);
        }
    }

    if (pos + 2 < len) {
        pos += (size_t)snprintf(buf + pos, len - pos, "]}");
    }

    return pos;
}
