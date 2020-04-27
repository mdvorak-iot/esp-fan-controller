#include <Arduino.h>
#include <esp_http_server.h>

static httpd_handle_t server;

esp_err_t getRootHandler(httpd_req_t *req)
{
    const char *HTML_START = "<!DOCTYPE html>\n"
                             "<html>"
                             "<head>"
                             "<title>RAD</title>"
                             "<link rel=\"stylesheet\" href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.4.1/css/bootstrap.min.css\">"
                             "<script src=\"https://www.chartjs.org/dist/2.9.3/Chart.min.js\"></script>"
                             "</head>"
                             "<body>"
                             "<div>"
                             "<canvas id=\"graph\"></canvas>"
                             "</div>"
                             "</body>"
                             "</html>";

    // TODO
    std::string html("<!DOCTYPE html><html><head><title>RAD</title></head><body>");
    html.append("Duty Cycle: ").append("<br/>");
    html.append("</body></html>");

    httpd_resp_send(req, html.c_str(), html.length());
    return ESP_OK;
}

void registerHandler(const char *uri, httpd_method_t method, esp_err_t (*handler)(httpd_req_t *r))
{
    httpd_uri_t def{
        .uri = uri,
        .method = HTTP_GET,
        .handler = handler,
        .user_ctx = nullptr,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &def));
}

void setupHttp()
{
    /* Generate default configuration */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    // Register URI handlers
    registerHandler("/", HTTP_GET, getRootHandler);
}
