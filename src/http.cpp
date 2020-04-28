#include <Arduino.h>
#include <sstream>
#include <iomanip>
#include <esp_http_server.h>
#include "Values.h"

static httpd_handle_t server;

static std::string ROOT_HTML = "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n    <meta charset=\"UTF-8\">\n    <title>RAD</title>\n    <link rel=\"stylesheet\" href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.4.1/css/bootstrap.min.css\"\n          crossorigin=\"anonymous\">\n    <script src=\"https://cdnjs.cloudflare.com/ajax/libs/moment.js/2.24.0/moment.min.js\"\n            crossorigin=\"anonymous\"></script>\n    <script src=\"https://www.chartjs.org/dist/2.9.3/Chart.min.js\" crossorigin=\"anonymous\"></script>\n    <style>body {\n        padding: 10px;\n    }</style>\n</head>\n<body>\n<div>\n    <canvas id=\"graph\" style=\"width: 100%; height: 300px;\"></canvas>\n</div>\n<div>\n    <p>\n        <label>Temperature</label>: <span id=\"temp\"></span>&nbsp;&nbsp;\n        <label>RPM</label>: <span id=\"rpm\"></span>&nbsp;&nbsp;\n        <label>Duty</label>: <span id=\"duty\"></span>&nbsp;&nbsp;\n        <label>Uptime</label>: <span id=\"uptime\"></span>\n</div>\n<script>\n    const MAX_ENTRIES = 300;\n\n    let chart = new Chart('graph', {\n        data: {\n            datasets: [\n                {\n                    label: 'Temperature',\n                    type: 'line',\n                    fill: false,\n                    pointRadius: 0,\n                    yAxisID: 'temp',\n                    borderColor: '#A03333',\n                    backgroundColor: '#A03333'\n                },\n                {\n                    label: 'Fan',\n                    type: 'line',\n                    fill: false,\n                    pointRadius: 0,\n                    yAxisID: 'rpm',\n                    borderColor: '#009900',\n                    backgroundColor: '#009900'\n                }\n            ]\n        },\n        options: {\n            maintainAspectRatio: false,\n            scales: {\n                xAxes: [{type: 'time', time: {minUnit: 'second'}, bounds: 'ticks'}],\n                yAxes: [{\n                    id: 'temp',\n                    scaleLabel: {\n                        display: true,\n                        labelString: '°C'\n                    },\n                    ticks: {beginAtZero: true}\n                }, {\n                    id: 'rpm',\n                    scaleLabel: {\n                        display: true,\n                        labelString: 'RPM'\n                    },\n                    ticks: {beginAtZero: true},\n                    position: 'right'\n                }]\n            },\n            tooltips: {\n                intersect: false,\n                mode: 'index'\n            }\n        }\n    });\n\n    chart.config.data.datasets[0].data.push({x: Date.now() - MAX_ENTRIES * 1000, y: 0});\n    chart.config.data.datasets[1].data.push({x: Date.now() - MAX_ENTRIES * 1000, y: 0});\n\n    function add(json) {\n        let now = Date.now();\n        chart.config.data.datasets[0].data.push({x: now, y: json.temp || 0});\n        chart.config.data.datasets[1].data.push({x: now, y: json.rpm || 0});\n        chart.update();\n\n        document.getElementById('temp').innerText = json.temp || 0;\n        document.getElementById('duty').innerText = json.duty || 0;\n        document.getElementById('rpm').innerText = json.rpm || 0;\n        document.getElementById('uptime').innerText = ((json.up || 0) / 1000).toFixed(0);\n    }\n\n    function update() {\n        fetch(new Request('/data'))\n            .then(response => {\n                if (response.status === 200) {\n                    return response.json();\n                }\n            })\n            .then(add)\n    }\n\n    setInterval(update, 1000);\n</script>\n</body>\n</html>\n";
;

esp_err_t getRootHandler(httpd_req_t *req)
{
    httpd_resp_send(req, ROOT_HTML.c_str(), ROOT_HTML.length());
    return ESP_OK;
}

esp_err_t getDataHandler(httpd_req_t *req)
{
    std::ostringstream json;
    
    json << "{\"temp\":" << std::fixed << std::setprecision(1) << Values::temperature.load() << ',';
    json << "\"rpm\":" << Values::rpm.load() << ',';
    json << "\"duty\":" << (int)Values::duty.load() << ',';
    json << "\"up\":" << millis();
    json << "}";

    auto resp = json.str();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp.c_str(), resp.length());
    return ESP_OK;
}

esp_err_t getMetricsHandler(httpd_req_t *req)
{
    std::ostringstream m;

    m << "# HELP hw_temp_celsius Temperature in Celsius\n";
    m << "# TYPE hw_temp_celsius gauge\n";
    m << "hw_temp_celsius{sensor=\"rad_intake\"} " << std::fixed << std::setprecision(1) << Values::temperature.load() << '\n';
    m << "# HELP hw_fan_rpm Fan RPM\n";
    m << "# TYPE hw_fan_rpm gauge\n";
    m << "hw_fan_rpm{sensor=\"rad\"} " << Values::rpm.load() << '\n';
    m << "# HELP hw_uptime Milliseconds from device power-up\n";
    m << "# TYPE hw_uptime counter\n";
    m << "hw_uptime{device=\"rad_esp\"} " << millis() << '\n';

    auto resp = m.str();
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, resp.c_str(), resp.length());
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
    registerHandler("/data", HTTP_GET, getDataHandler);
    registerHandler("/metrics", HTTP_GET, getMetricsHandler);
}
