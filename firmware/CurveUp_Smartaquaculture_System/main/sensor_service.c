#include "sensor_service.h"

#include "board_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "math.h"
#include "thresholds.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "SENSOR_SVC";

static sensor_snapshot_t s_snap;
static SemaphoreHandle_t s_lock;

typedef struct {
    const char *id;
    const char *label;
    const char *species;
    bool live;
} tank_meta_t;

static const tank_meta_t TANKS[] = {
    { "L1", "L1 · Electric Blue", "Procambarus alleni", true },
    { "L2", "L2 · Red Claw", "Procambarus clarkii", false },
    { "L3", "L3 · Electric Blue", "Procambarus alleni", false },
};

static const char *reading_state(const sensor_reading_t *r)
{
    if (!r->enabled || !r->connected) {
        return "OFF";
    }
    return "ACTIVE";
}

static void print_dashboard(const sensor_snapshot_t *s)
{
    ESP_LOGI(TAG, "======== %s ========", FIRMWARE_NAME);
    if (s->temperature.enabled) {
        if (s->temperature.connected && !isnan(s->temperature.celsius)) {
            ESP_LOGI(TAG, "Temp        : %.2f C  (%.3f V, raw %d)",
                     s->temperature.celsius, s->temperature.volts, s->temperature.raw);
        } else {
            ESP_LOGI(TAG, "Temp        : OFF / disconnected (%.3f V)", s->temperature.volts);
        }
    }
    if (s->turbidity.enabled) {
        const param_band_t turb = thresholds_turbidity_level(s->turbidity.volts);
        ESP_LOGI(TAG, "Turbidity   : %s  %.3f V  raw %d  [%s]",
                 s->turbidity.connected ? "OK" : "OFF",
                 s->turbidity.volts, s->turbidity.raw, turb.label);
    }
    if (s->water_level.enabled) {
        const int pct = (int)(s->water_level.percent + 0.5f);
        ESP_LOGI(TAG, "Water level : %s  %d%%  %.3f V  raw %d  [%s]",
                 s->water_level.connected ? "OK" : "OFF",
                 pct, s->water_level.volts, s->water_level.raw,
                 water_level_label(pct));
    }

    char alerts[512];
    const bool temp_ok = s->temperature.connected && !isnan(s->temperature.celsius);
    const int level_pct = (int)(s->water_level.percent + 0.5f);
    int n = thresholds_collect_alerts(
        s->temperature.celsius, temp_ok, s->turbidity.volts, level_pct,
        s->water_level.volts, alerts, sizeof(alerts));
    if (n > 0) {
        ESP_LOGW(TAG, "ALERTS (%d): %s", n, alerts);
    } else {
        ESP_LOGI(TAG, "Alerts      : none");
    }

#if CALIBRATION_LOG
    ESP_LOGI(TAG, "CAL | temp_c=%.2f temp_v=%.3f | level_pct=%.0f level_v=%.3f | turb_v=%.3f",
             s->temperature.celsius, s->temperature.volts,
             s->water_level.percent, s->water_level.volts,
             s->turbidity.volts);
#endif
    ESP_LOGI(TAG, "================================");
}

static void sensor_task(void *arg)
{
    (void)arg;
    sensor_snapshot_t local;

    while (true) {
        sensors_read_all(&local);
        if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
            s_snap = local;
            xSemaphoreGive(s_lock);
        }
        print_dashboard(&local);
        vTaskDelay(pdMS_TO_TICKS(SENSOR_SAMPLE_MS));
    }
}

esp_err_t sensor_service_init(void)
{
    sensors_init();
    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }
    memset(&s_snap, 0, sizeof(s_snap));

    BaseType_t ok = xTaskCreate(sensor_task, "sensors", 4096, NULL, 5, NULL);
    if (ok != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Sensor service started");
    return ESP_OK;
}

void sensor_service_get_snapshot(sensor_snapshot_t *out)
{
    if (out == NULL) {
        return;
    }
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
        *out = s_snap;
        xSemaphoreGive(s_lock);
    }
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
    const bool temp_ok = snap->temperature.connected && !isnan(snap->temperature.celsius);
    const param_band_t temp_band = thresholds_temp_level(snap->temperature.celsius, temp_ok);
    const param_band_t turb_band = thresholds_turbidity_level(snap->turbidity.volts);
    const int level_pct = (int)(snap->water_level.percent + 0.5f);
    const param_band_t level_band = thresholds_water_level(level_pct);

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
            snap->temperature.celsius,
            snap->temperature.volts,
            reading_state(&snap->temperature),
            temp_band.label,
            temp_band.color,
            snap->turbidity.raw,
            snap->turbidity.volts,
            reading_state(&snap->turbidity),
            turb_band.label,
            turb_band.color,
            level_pct,
            snap->water_level.raw,
            snap->water_level.volts,
            water_level_label(level_pct),
            reading_state(&snap->water_level),
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
            snap->temperature.volts,
            reading_state(&snap->temperature),
            temp_band.label,
            temp_band.color,
            snap->turbidity.raw,
            snap->turbidity.volts,
            reading_state(&snap->turbidity),
            turb_band.label,
            turb_band.color,
            level_pct,
            snap->water_level.raw,
            snap->water_level.volts,
            water_level_label(level_pct),
            reading_state(&snap->water_level),
            level_band.label,
            level_band.color);
    }
}

size_t sensor_service_json(char *buf, size_t len)
{
    if (buf == NULL || len < 32) {
        return 0;
    }

    sensor_snapshot_t snap;
    memset(&snap, 0, sizeof(snap));
    sensor_service_get_snapshot(&snap);

    int connected = 0;
    int active = 0;

    if (snap.temperature.enabled && snap.temperature.connected) {
        connected++;
        active++;
    }
    if (snap.turbidity.enabled && snap.turbidity.connected) {
        connected++;
        active++;
    }
    if (snap.water_level.enabled && snap.water_level.connected) {
        connected++;
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

    char alerts[512];
    const bool temp_ok = snap.temperature.connected && !isnan(snap.temperature.celsius);
    const int level_pct = (int)(snap.water_level.percent + 0.5f);
    thresholds_collect_alerts(
        snap.temperature.celsius, temp_ok, snap.turbidity.volts, level_pct,
        snap.water_level.volts, alerts, sizeof(alerts));

    if (pos + strlen(alerts) + 16 < len) {
        pos += (size_t)snprintf(buf + pos, len - pos, "],\"alerts\":%s}", alerts);
    } else if (pos + 2 < len) {
        pos += (size_t)snprintf(buf + pos, len - pos, "]}");
    }

    return pos;
}
