#include "aq_mqtt.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "mqtt_config.h"
#include "sensor_service.h"

static const char *TAG = "AQ_MQTT";

#if MQTT_ENABLED

static esp_mqtt_client_handle_t s_client;
static volatile bool s_connected;

static void publish_readings(void)
{
    if (!s_connected || s_client == NULL) {
        return;
    }

    char json[3072];
    size_t len = sensor_service_json(json, sizeof(json));
    if (len == 0) {
        return;
    }

    esp_mqtt_client_publish(s_client, MQTT_TOPIC_READINGS, json, (int)len, MQTT_QOS, MQTT_RETAIN_READINGS);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;

    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "Connected to broker");
        esp_mqtt_client_publish(s_client, MQTT_TOPIC_STATUS, "online", 6, MQTT_QOS, MQTT_RETAIN_STATUS);
        publish_readings();
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "Disconnected from broker");
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error");
        break;
    default:
        break;
    }
}

static void mqtt_publish_task(void *arg)
{
    (void)arg;

    while (true) {
        publish_readings();
        vTaskDelay(pdMS_TO_TICKS(MQTT_PUBLISH_INTERVAL_MS));
    }
}

esp_err_t aq_mqtt_start(void)
{
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials.client_id = MQTT_CLIENT_ID,
    };

    s_client = esp_mqtt_client_init(&cfg);
    if (s_client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_err_t err = esp_mqtt_client_start(s_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "MQTT start failed: %s", esp_err_to_name(err));
        return err;
    }

    xTaskCreate(mqtt_publish_task, "mqtt_pub", 4096, NULL, 4, NULL);
    ESP_LOGI(TAG, "Publishing to %s", MQTT_TOPIC_READINGS);
    return ESP_OK;
}

void aq_mqtt_stop(void)
{
    if (s_client != NULL) {
        esp_mqtt_client_publish(s_client, MQTT_TOPIC_STATUS, "offline", 7, MQTT_QOS, MQTT_RETAIN_STATUS);
        esp_mqtt_client_stop(s_client);
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
    }
    s_connected = false;
}

bool aq_mqtt_is_connected(void)
{
    return s_connected;
}

#else

esp_err_t aq_mqtt_start(void)
{
    ESP_LOGI(TAG, "MQTT disabled (MQTT_ENABLED=0)");
    return ESP_OK;
}

void aq_mqtt_stop(void) {}

bool aq_mqtt_is_connected(void)
{
    return false;
}

#endif
