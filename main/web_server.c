#include "web_server.h"
#include <esp_log.h>
#include <status_led.h>

static const char TAG[] = "web_server";

static httpd_handle_t httpd = NULL;

static esp_err_t web_server_on_open(httpd_handle_t hd, int sockfd)
{
    status_led_set_interval_for(STATUS_LED_DEFAULT, 0, true, 50, false);
    return ESP_OK;
}

esp_err_t web_server_start()
{
    httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();
    httpd_config.open_fn = web_server_on_open;
    return httpd_start(&httpd, &httpd_config);
}

esp_err_t web_server_register_handler(const char *uri, httpd_method_t method, esp_err_t (*handler)(httpd_req_t *r), void *arg)
{
    ESP_LOGI(TAG, "registering %d %s", method, uri);

    httpd_uri_t def = {
        .uri = uri,
        .method = method,
        .handler = handler,
        .user_ctx = arg,
    };

    return httpd_register_uri_handler(httpd, &def);
}
