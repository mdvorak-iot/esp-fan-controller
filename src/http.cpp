#include <Arduino.h>
#include <sstream>
#include <iomanip>
#include <map>
#include <esp_http_server.h>
#include <ArduinoJson.h>
#include "httpd_json.h"
#include "app_config.h"
#include "state.h"
#include "version.h"
#include "rpm_counter.h"
#include "app_config.h"
#include "app_config_json.h"
#include "app_temps.h"

static httpd_handle_t server;
static appconfig::app_config config;
static std::vector<uint16_t> rpms;
static std::vector<apptemps::temperature_sensor> temps;
static std::string hardware;
static std::vector<std::string> sensorNames;

static std::string ROOT_HTML = "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n    <meta charset=\"UTF-8\">\n    <title>RAD</title>\n    <link rel=\"stylesheet\" href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.4.1/css/bootstrap.min.css\"\n          crossorigin=\"anonymous\">\n    <script src=\"https://cdnjs.cloudflare.com/ajax/libs/moment.js/2.24.0/moment.min.js\"\n            crossorigin=\"anonymous\"></script>\n    <script src=\"https://www.chartjs.org/dist/2.9.3/Chart.min.js\" crossorigin=\"anonymous\"></script>\n    <style>\n        body {\n            padding: 10px;\n        }\n\n        .graph-values label {\n            margin-left: 2em;\n        }\n    </style>\n</head>\n<body>\n<div class=\"container-fluid\">\n    <div class=\"row\">\n        <div class=\"col-sm\">\n            <canvas id=\"graph\" style=\"width: 100%; height: 300px;\"></canvas>\n        </div>\n    </div>\n    <div class=\"row\">\n        <div class=\"col-sm\">\n            <p class='graph-values'>\n                <label>Temperature:</label> <span id=\"temp\"></span>\n                <label>RPM:</label> <span id=\"rpm\"></span>\n                <label>Duty:</label> <span id=\"duty\"></span>\n                <label>Uptime:</label> <span id=\"uptime\"></span>\n        </div>\n    </div>\n    <div class=\"row\">\n        <div class='col-sm'>\n            <form onsubmit='updateConfig()'>\n                <div class=\"form-group\">\n                    <label for='config'>Configuration</label>\n                    <textarea id='config' class=\"form-control form-control-sm\" rows='30'></textarea>\n                </div>\n                <button type='submit' class='btn btn-primary'>Save and Reboot</button>\n            </form>\n        </div>\n    </div>\n</div>\n<script>\n    fetch(new Request(\"/config\"))\n        .then(response => {\n            if (response.status === 200) {\n                return response.json();\n            }\n        })\n        .then(json => document.getElementById(\"config\").value = JSON.stringify(json, null, 2))\n        .catch(alert);\n\n    function updateConfig() {\n        let json = JSON.parse(document.getElementById(\"config\").value);\n        fetch(new Request(\"/config\", {method: \"PUT\", body: json}))\n            .then(response => {\n                if (response.status !== 200) {\n                    alert(response.status + \" \" + response.statusText);\n                }\n            })\n            .catch(alert);\n    }\n</script>\n<script>\n    const MAX_ENTRIES = 300;\n\n    let chart = new Chart('graph', {\n        data: {\n            datasets: [\n                {\n                    label: 'Temperature',\n                    type: 'line',\n                    fill: false,\n                    pointRadius: 0,\n                    yAxisID: 'temp',\n                    borderColor: '#A03333',\n                    backgroundColor: '#A03333'\n                },\n                {\n                    label: 'Fan',\n                    type: 'line',\n                    fill: false,\n                    pointRadius: 0,\n                    yAxisID: 'rpm',\n                    borderColor: '#009900',\n                    backgroundColor: '#009900'\n                }\n            ]\n        },\n        options: {\n            maintainAspectRatio: false,\n            scales: {\n                xAxes: [{type: 'time', time: {minUnit: 'second'}, bounds: 'ticks'}],\n                yAxes: [{\n                    id: 'temp',\n                    scaleLabel: {\n                        display: true,\n                        labelString: '°C'\n                    },\n                    ticks: {beginAtZero: true}\n                }, {\n                    id: 'rpm',\n                    scaleLabel: {\n                        display: true,\n                        labelString: 'RPM'\n                    },\n                    ticks: {beginAtZero: true},\n                    position: 'right'\n                }]\n            },\n            tooltips: {\n                intersect: false,\n                mode: 'index'\n            }\n        }\n    });\n\n    chart.config.data.datasets[0].data.push({x: Date.now() - MAX_ENTRIES * 1000, y: 0});\n    chart.config.data.datasets[1].data.push({x: Date.now() - MAX_ENTRIES * 1000, y: 0});\n\n    function add(json) {\n        let now = Date.now();\n        chart.config.data.datasets[0].data.push({x: now, y: json.temp || 0});\n        chart.config.data.datasets[1].data.push({x: now, y: json.rpm || 0});\n\n        while (chart.config.data.datasets[0].data.length > MAX_ENTRIES) {\n            chart.config.data.datasets[0].data.shift();\n            chart.config.data.datasets[1].data.shift();\n        }\n\n        chart.update();\n\n        document.getElementById('temp').innerText = json.temp || 0;\n        document.getElementById('duty').innerText = json.duty || 0;\n        document.getElementById('rpm').innerText = json.rpm || 0;\n        document.getElementById('uptime').innerText = ((json.up || 0) / 1000).toFixed(0);\n    }\n\n    function update() {\n        fetch(new Request('/data'))\n            .then(response => {\n                if (response.status === 200) {\n                    return response.json();\n                }\n            })\n            .then(add)\n            .finally(() => setTimeout(update, 2000));\n    }\n\n    update();\n</script>\n</body>\n</html>\n";

esp_err_t getRootHandler(httpd_req_t *req)
{
    httpd_resp_send(req, ROOT_HTML.c_str(), ROOT_HTML.length());
    return ESP_OK;
}

esp_err_t getDataHandler(httpd_req_t *req)
{
    StaticJsonDocument<JSON_OBJECT_SIZE(6) + 26> json;

    json["hardware"] = hardware;
    json["up"] = millis();
    //json["temp"] = ;
    //json["duty"] = ;
    json["rpm"] = rpmcounter::rpm_counter_value();

    HttpdResponseWriter writer(req);
    httpd_resp_set_type(req, "application/json");
    serializeJson(json, writer);
    return writer.close();
}

esp_err_t getMetricsHandler(httpd_req_t *req)
{
    std::ostringstream m;

    m << "# TYPE esp_celsius gauge\n";
    apptemps::temperature_values(temps);
    for (size_t i = 0; i < temps.size(); i++)
    {
        m << "esp_celsius{hardware=\"" << hardware << "\",sensor=\"" << sensorNames[i] << "\",address=\"" << temps[i].address << "\"} " << std::fixed << std::setprecision(3) << temps[i].values << '\n';
    }

    // Get RPMs
    m << "# TYPE esp_rpm gauge\n";
    rpmcounter::rpm_counter_values(rpms);
    for (size_t i = 0; i < rpms.size(); i++)
    {
        m << "esp_rpm{hardware=\"" << hardware << "\",sensor=\"Fan " << i << "\"} " << rpms[i] << '\n';
    }

    auto resp = m.str();
    ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_type(req, "text/plain"));
    return httpd_resp_send(req, resp.c_str(), resp.length());
}

esp_err_t getInfoHandler(httpd_req_t *req)
{
    StaticJsonDocument<JSON_OBJECT_SIZE(3) + 26> json;
    json["name"] = hardware;
    json["version"] = VERSION;

    HttpdResponseWriter writer(req);
    ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_type(req, "application/json"));
    serializeJson(json, writer);
    return writer.close();
}

esp_err_t getConfigHandler(httpd_req_t *req)
{
    // Prepare
    StaticJsonDocument<appconfig::APP_CONFIG_JSON_SIZE> json;
    appconfig::app_config_to_json(config, json);

    // Serialize
    HttpdResponseWriter writer(req);
    ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_type(req, "application/json"));
    serializeJson(json, writer);
    return writer.close();
}

void delayedRestart(void *)
{
    log_i("restarting in 1000 ms");
    vTaskDelay(1000);
    log_i("restart for reconfiguration");
    esp_restart();
}

esp_err_t putConfigHandler(httpd_req_t *req)
{
    // Read request
    HttpdRequestReader reader(req);
    StaticJsonDocument<appconfig::APP_CONFIG_JSON_SIZE> json;

    if (req->content_len == 0 || deserializeJson(json, reader) != DeserializationError::Ok)
    {
        return httpd_resp_set_status(req, "400 Bad Request");
    }

    // Deserialize
    appconfig::app_config newConfig = appconfig::app_config_from_json(json);

    // Update
    esp_err_t err = appconfig::app_config_update(newConfig);
    if (err != ESP_OK)
    {
        // Failed
        return err;
    }

    // Delayed reboot
    xTaskCreate(delayedRestart, "reboot", 1024, nullptr, tskIDLE_PRIORITY + 10, nullptr);

    // Success
    return httpd_resp_set_status(req, "204 No Content");
}

void registerHandler(const char *uri, httpd_method_t method, esp_err_t (*handler)(httpd_req_t *r))
{
    log_i("registering %d %s", method, uri);
    httpd_uri_t def{
        .uri = uri,
        .method = method,
        .handler = handler,
        .user_ctx = nullptr,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &def));
}

void setupHttp(const appconfig::app_config &config, const std::vector<std::string> &sensorNames)
{
    // Store config
    ::config = config;
    ::sensorNames = sensorNames;
    ::hardware = config.data.hardware_name;

    // Generate default configuration
    httpd_config_t httpd = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_ERROR_CHECK(httpd_start(&server, &httpd));

    // Register URI handlers
    registerHandler("/", HTTP_GET, getRootHandler);
    registerHandler("/data", HTTP_GET, getDataHandler);
    registerHandler("/metrics", HTTP_GET, getMetricsHandler);
    registerHandler("/info", HTTP_GET, getInfoHandler);
    registerHandler("/config", HTTP_GET, getConfigHandler);
    registerHandler("/config", HTTP_PUT, putConfigHandler);
}
