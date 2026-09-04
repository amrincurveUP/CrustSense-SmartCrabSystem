#include "aq_http.h"
#include "aq_mqtt.h"
#include "board_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "irrigation.h"
#include "mqtt_config.h"
#include "nvs_flash.h"
#include "sensor_service.h"
#include "wifi_config.h"
#include "wifi_setup.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(300));

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " %s", FIRMWARE_NAME);
    ESP_LOGI(TAG, " Board: %s", BOARD_NAME);
    ESP_LOGI(TAG, " Device: %s", MQTT_DEVICE_ID);
    ESP_LOGI(TAG, "========================================");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(ret);
    }

    ESP_ERROR_CHECK(sensor_service_init());

#if WIFI_ENABLED
    if (wifi_setup_init() == ESP_OK) {
        ESP_LOGI(TAG, "WiFi OK — IP %s", wifi_setup_get_ip());
#if HTTP_INGEST_ENABLED
        ESP_ERROR_CHECK(aq_http_start());
        ESP_LOGI(TAG, "Dashboard sync: HTTP → %s", CLOUD_INGEST_URL);
        ESP_LOGI(TAG, "Open dashboard: http://%s:%d/", CLOUD_HOST, CLOUD_API_PORT);
#endif
#if IRRIGATION_ENABLED
        ESP_ERROR_CHECK(irrigation_start());
        ESP_LOGI(TAG, "Irrigation control: poll → %s", CLOUD_IRRIGATION_CMD_URL);
#endif
#if MQTT_ENABLED
        ESP_ERROR_CHECK(aq_mqtt_start());
        ESP_LOGI(TAG, "MQTT also enabled → %s", MQTT_BROKER_URI);
#endif
    } else {
        ESP_LOGE(TAG, "WiFi failed — edit main/wifi_config.h and reflash");
    }
#else
    ESP_LOGI(TAG, "WiFi disabled — sensor bring-up only (serial monitor)");
#endif

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
