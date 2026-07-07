idf_component_register(SRCS "sensor_status.c" "ntc_temp.c" "sensor_adc.c" "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_adc driver)