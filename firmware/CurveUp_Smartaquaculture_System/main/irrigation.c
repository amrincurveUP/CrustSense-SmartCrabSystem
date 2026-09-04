#include "irrigation.h"

#include "board_config.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_config.h"
#include "pump.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "IRRIGATION";

#if IRRIGATION_ENABLED

static void post_status(bool clear_relay_test)
{
    char body[220];
    snprintf(body, sizeof(body),
             "{\"device_id\":\"%s\",\"running\":%s,\"run_ms\":%u,\"test_done\":%s}",
             MQTT_DEVICE_ID,
             pump_is_on() ? "true" : "false",
             (unsigned)pump_run_ms(),
             clear_relay_test ? "true" : "false");

    esp_http_client_config_t cfg = {
        .url = CLOUD_IRRIGATION_STATUS_URL,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 4000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        return;
    }
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "X-Device-Id", MQTT_DEVICE_ID);
    esp_http_client_set_post_field(client, body, (int)strlen(body));
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "status post failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

static bool fetch_desired_pump(bool *out_on, uint32_t *out_max_ms, bool *out_test)
{
    char buf[256];
    int len = 0;
    *out_test = false;

    esp_http_client_config_t cfg = {
        .url = CLOUD_IRRIGATION_CMD_URL,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 4000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        return false;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return false;
    }
    (void)esp_http_client_fetch_headers(client);
    len = esp_http_client_read(client, buf, sizeof(buf) - 1);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (len <= 0) {
        return false;
    }
    buf[len] = '\0';

    *out_on = (strstr(buf, "\"pump\":true") != NULL);
    *out_test = (strstr(buf, "\"relay_test\":true") != NULL);
    *out_max_ms = PUMP_MAX_RUN_MS;
    const char *m = strstr(buf, "\"max_run_ms\":");
    if (m != NULL) {
        unsigned v = 0;
        if (sscanf(m, "\"max_run_ms\":%u", &v) == 1 && v > 0) {
            *out_max_ms = v;
        }
    }
    return true;
}

static void post_clear_pump_command(void)
{
    const char *body = "{\"pump\":false,\"relay_test\":false}";
    esp_http_client_config_t cfg = {
        .url = CLOUD_IRRIGATION_CMD_URL,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 4000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        return;
    }
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "X-Device-Id", MQTT_DEVICE_ID);
    esp_http_client_set_post_field(client, body, (int)strlen(body));
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "clear command failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

static void irrigation_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(2500));
    ESP_LOGI(TAG, "Polling %s", CLOUD_IRRIGATION_CMD_URL);

    bool latched_off = false; /* after safety timeout, stay off until backend pump=false */

    while (true) {
        bool want = false;
        bool do_test = false;
        uint32_t max_ms = PUMP_MAX_RUN_MS;
        if (fetch_desired_pump(&want, &max_ms, &do_test)) {
            if (do_test) {
                latched_off = false;
                pump_run_click_test(8);
                post_status(true);
                vTaskDelay(pdMS_TO_TICKS(IRRIGATION_POLL_MS));
                continue;
            }

            if (!want) {
                latched_off = false;
            }

            if (latched_off) {
                want = false;
            }

            if (want && pump_is_on() && pump_run_ms() >= max_ms) {
                ESP_LOGW(TAG, "Max run %u ms — latch OFF and clear backend command",
                         (unsigned)max_ms);
                want = false;
                latched_off = true;
                pump_set(false);
                post_clear_pump_command();
                post_status(false);
                vTaskDelay(pdMS_TO_TICKS(IRRIGATION_POLL_MS));
                continue;
            }

            pump_set(want);
            post_status(false);
        } else {
            /* Lost backend — fail safe OFF */
            ESP_LOGW(TAG, "Command fetch failed — pump OFF");
            pump_set(false);
            latched_off = true;
        }

        vTaskDelay(pdMS_TO_TICKS(IRRIGATION_POLL_MS));
    }
}

esp_err_t irrigation_start(void)
{
    ESP_ERROR_CHECK(pump_init());
    BaseType_t ok = xTaskCreate(irrigation_task, "irrigation", 6144, NULL, 4, NULL);
    return (ok == pdPASS) ? ESP_OK : ESP_ERR_NO_MEM;
}

#else

esp_err_t irrigation_start(void)
{
    ESP_LOGI(TAG, "Irrigation disabled");
    return ESP_OK;
}

#endif
