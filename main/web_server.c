#include "web_server.h"
#include <esp_log.h>

static const char TAG[] = "web_server";

static httpd_handle_t httpd = NULL;

esp_err_t web_server_start()
{
    httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();
    return httpd_start(&httpd, &httpd_config);
}

esp_err_t web_server_stop()
{
    return httpd_stop(&httpd);
}

esp_err_t web_server_register_handler(const char *uri, httpd_method_t method, esp_err_t (*handler)(__unused httpd_req_t *r), void *arg)
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
