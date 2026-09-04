#pragma once

/*
 * WiFi — ESP32 and your Mac (MQTT broker / dashboard) must be on the same LAN.
 *
 * OPEN network: WIFI_IS_OPEN = 1, WIFI_PASS = ""
 * Password network: WIFI_IS_OPEN = 0, set WIFI_PASS
 */

#define WIFI_SSID      "TP-Link_2.4GHz_A8E1C8"
#define WIFI_PASS      ""
#define WIFI_IS_OPEN   1

#define WIFI_MAX_RETRY 15
#define WIFI_ENABLED   1
