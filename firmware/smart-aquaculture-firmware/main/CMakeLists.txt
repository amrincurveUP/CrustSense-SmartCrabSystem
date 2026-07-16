idf_component_register(
    SRCS
        "main.c"
        "sensor_adc.c"
        "ntc_temp.c"
        "water_level.c"
        "sensor_status.c"
        "telemetry.c"
        "sensor_service.c"
        "thresholds.c"
        "wifi_setup.c"
        "web_server.c"
        "aq_mqtt.c"
    INCLUDE_DIRS "."
    REQUIRES esp_adc driver esp_wifi esp_http_server nvs_flash esp_netif esp_event mqtt
    EMBED_FILES
        "web/index.html"
        "assets/logo.png"
        "assets/electric-blue.jpg"
        "assets/red-crayfish.jpg"
)
