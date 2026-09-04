#include "aq_http.h"

#include <string.h>

#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_config.h"
#include "sensor_service.h"

static const char *TAG = "AQ_HTTP";

#if HTTP_INGEST_ENABLED

static esp_err_t post_readings(void)
{
    char json[3072];
    size_t len = sensor_service_json(json, sizeof(json));
    if (len == 0) {
        return ESP_ERR_INVALID_SIZE;
    }

    esp_http_client_config_t cfg = {
        .url = CLOUD_INGEST_URL,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "X-Device-Id", MQTT_DEVICE_ID);
    esp_http_client_set_post_field(client, json, (int)len);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        if (status >= 200 && status < 300) {
            ESP_LOGI(TAG, "Posted %u bytes → %s (%d)", (unsigned)len, CLOUD_INGEST_URL, status);
        } else {
            ESP_LOGW(TAG, "HTTP ingest status %d", status);
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "HTTP ingest failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}

static void http_publish_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(1500));

    while (true) {
        post_readings();
        vTaskDelay(pdMS_TO_TICKS(CLOUD_PUBLISH_MS));
    }
}

esp_err_t aq_http_start(void)
{
    BaseType_t ok = xTaskCreate(http_publish_task, "http_pub", 6144, NULL, 4, NULL);
    if (ok != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "HTTP ingest every %d ms → %s", CLOUD_PUBLISH_MS, CLOUD_INGEST_URL);
    return ESP_OK;
}

#else

esp_err_t aq_http_start(void)
{
    ESP_LOGI(TAG, "HTTP ingest disabled");
    return ESP_OK;
}

#endif
