#pragma once

/*
 * MQTT cloud settings — edit before flashing.
 * Broker must be reachable on the same LAN as the ESP32 (e.g. your PC or cloud VM).
 *
 * Topic layout:
 *   smart-aquaculture/{device_id}/readings   — sensor JSON payload
 *   smart-aquaculture/{device_id}/status     — online/offline (retained)
 */

#define MQTT_ENABLED           1

/* LAN broker — set to your PC IP running Mosquitto / docker-compose */
#define MQTT_BROKER_URI        "mqtt://192.168.1.100:1883"

#define MQTT_DEVICE_ID         "smart-aq-l1"
#define MQTT_CLIENT_ID         "smart-aq-l1-esp32"

#define MQTT_TOPIC_READINGS    "smart-aquaculture/smart-aq-l1/readings"
#define MQTT_TOPIC_STATUS      "smart-aquaculture/smart-aq-l1/status"

#define MQTT_PUBLISH_INTERVAL_MS  5000
#define MQTT_QOS               1
#define MQTT_RETAIN_READINGS   0
#define MQTT_RETAIN_STATUS     1
