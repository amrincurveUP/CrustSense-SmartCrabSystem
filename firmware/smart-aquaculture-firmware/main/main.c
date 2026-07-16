#include "aq_mqtt.h"
#include "board_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sensor_service.h"
#include "web_server.h"
#include "wifi_setup.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, " Smart Aquaculture — WiFi Monitor");
    ESP_LOGI(TAG, "=================================");

    ESP_ERROR_CHECK(sensor_service_init());

    if (wifi_setup_init() == ESP_OK) {
        ESP_ERROR_CHECK(web_server_start());
        ESP_ERROR_CHECK(aq_mqtt_start());
        ESP_LOGI(TAG, "Open dashboard on laptop/tablet:");
        ESP_LOGI(TAG, "  http://%s/", wifi_setup_get_ip());
    } else {
        ESP_LOGE(TAG, "WiFi failed — edit main/wifi_config.h and reflash");
    }

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
