#pragma once

/*
 * Cloud sync — Mac running backend (dashboard + API).
 *
 * Prefer LAN IP when Bonjour/mDNS is unreliable on ESP.
 * Also keep crabit.local working when dns-sd advertises correctly.
 * Update IP if Mac WiFi address changes: ipconfig getifaddr en0
 */

#define CLOUD_HOST             "192.168.1.10"
#define CLOUD_API_PORT         3000
#define CLOUD_INGEST_URL       "http://192.168.1.10:3000/api/ingest"
#define CLOUD_PUBLISH_MS       5000

#define MQTT_ENABLED           0   /* HTTP ingest preferred (macOS firewall blocks :1883) */

#define MQTT_BROKER_URI        "mqtt://192.168.1.10:1883"
#define MQTT_DEVICE_ID         "curveup-s3-1"
#define MQTT_CLIENT_ID         "curveup-s3-1-esp32"
#define MQTT_TOPIC_READINGS    "smart-aquaculture/curveup-s3-1/readings"
#define MQTT_TOPIC_STATUS      "smart-aquaculture/curveup-s3-1/status"
#define MQTT_PUBLISH_INTERVAL_MS  5000
#define MQTT_QOS               1
#define MQTT_RETAIN_READINGS   0
#define MQTT_RETAIN_STATUS     1

#define HTTP_INGEST_ENABLED    1

/* Irrigation control (dashboard → backend → ESP polls) */
#define IRRIGATION_ENABLED     1
#define CLOUD_IRRIGATION_CMD_URL    "http://192.168.1.10:3000/api/irrigation/command"
#define CLOUD_IRRIGATION_STATUS_URL "http://192.168.1.10:3000/api/irrigation/status"
#define IRRIGATION_POLL_MS     2000
