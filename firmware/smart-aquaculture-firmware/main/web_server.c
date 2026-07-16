#include "web_server.h"

#include <string.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "sensor_service.h"
#include "wifi_setup.h"

static const char *TAG = "WEB";
static httpd_handle_t s_server;

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

extern const uint8_t logo_png_start[] asm("_binary_logo_png_start");
extern const uint8_t logo_png_end[] asm("_binary_logo_png_end");

extern const uint8_t electric_blue_jpg_start[] asm("_binary_electric_blue_jpg_start");
extern const uint8_t electric_blue_jpg_end[] asm("_binary_electric_blue_jpg_end");

extern const uint8_t red_crayfish_jpg_start[] asm("_binary_red_crayfish_jpg_start");
extern const uint8_t red_crayfish_jpg_end[] asm("_binary_red_crayfish_jpg_end");

static esp_err_t send_embedded(httpd_req_t *req, const char *type,
                               const uint8_t *start, const uint8_t *end)
{
    const size_t len = (size_t)(end - start);
    httpd_resp_set_type(req, type);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=86400");
    return httpd_resp_send(req, (const char *)start, len);
}

static esp_err_t root_handler(httpd_req_t *req)
{
    return send_embedded(req, "text/html", index_html_start, index_html_end);
}

static esp_err_t logo_handler(httpd_req_t *req)
{
    return send_embedded(req, "image/png", logo_png_start, logo_png_end);
}

static esp_err_t electric_blue_handler(httpd_req_t *req)
{
    return send_embedded(req, "image/jpeg", electric_blue_jpg_start, electric_blue_jpg_end);
}

static esp_err_t red_crayfish_handler(httpd_req_t *req)
{
    return send_embedded(req, "image/jpeg", red_crayfish_jpg_start, red_crayfish_jpg_end);
}

static esp_err_t api_handler(httpd_req_t *req)
{
    char json[3072];
    sensor_service_json(json, sizeof(json));

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

esp_err_t web_server_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 12;
    cfg.lru_purge_enable = true;
    cfg.stack_size = 8192;

    esp_err_t err = httpd_start(&s_server, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP server start failed");
        return err;
    }

    const httpd_uri_t routes[] = {
        { .uri = "/",                    .method = HTTP_GET, .handler = root_handler },
        { .uri = "/logo.png",            .method = HTTP_GET, .handler = logo_handler },
        { .uri = "/electric-blue.jpg",   .method = HTTP_GET, .handler = electric_blue_handler },
        { .uri = "/red-crayfish.jpg",    .method = HTTP_GET, .handler = red_crayfish_handler },
        { .uri = "/api/readings",        .method = HTTP_GET, .handler = api_handler },
    };

    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &routes[i]));
    }

    ESP_LOGI(TAG, "Dashboard: http://%s/", wifi_setup_get_ip());
    return ESP_OK;
}
