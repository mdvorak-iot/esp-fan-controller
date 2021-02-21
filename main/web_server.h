#pragma once
#include <esp_http_server.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t web_server_start();

esp_err_t web_server_stop();

esp_err_t web_server_register_handler(const char *uri, httpd_method_t method, esp_err_t (*handler)(httpd_req_t *r), void *arg);

#ifdef __cplusplus
}
#endif
