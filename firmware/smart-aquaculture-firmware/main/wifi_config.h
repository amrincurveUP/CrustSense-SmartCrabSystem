#pragma once

/*
 * WiFi settings — edit before flashing.
 * ESP32 and laptop/tablet must be on the same network.
 *
 * OPEN network (no password, e.g. company guest WiFi):
 *   - Set WIFI_SSID to the exact network name
 *   - Set WIFI_PASS to ""  (empty)
 *   - Set WIFI_IS_OPEN to 1
 *
 * Password-protected network:
 *   - Set WIFI_SSID and WIFI_PASS
 *   - Set WIFI_IS_OPEN to 0
 */

#define WIFI_SSID      "TP-Link_2.4GHz_A8E1C8"
#define WIFI_PASS      ""
#define WIFI_IS_OPEN   1

#define WIFI_MAX_RETRY 15
