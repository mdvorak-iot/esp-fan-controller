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

static const size_t CONFIG_JSON_SIZE = JSON_OBJECT_SIZE(12 + 3 * appconfig::APP_CONFIG_MAX_SENSORS + appconfig::APP_CONFIG_MAX_RPM) + 26;

static httpd_handle_t server;
// NOTe these static vars expect server to be single-threaded
static std::vector<uint16_t> rpms;
static std::string hardware;
static std::vector<std::string> sensorNames;

static std::string ROOT_HTML = "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n    <meta charset=\"UTF-8\">\n    <title>RAD</title>\n    <link rel=\"stylesheet\" href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.4.1/css/bootstrap.min.css\"\n          crossorigin=\"anonymous\">\n    <script src=\"https://cdnjs.cloudflare.com/ajax/libs/moment.js/2.24.0/moment.min.js\"\n            crossorigin=\"anonymous\"></script>\n    <script src=\"https://www.chartjs.org/dist/2.9.3/Chart.min.js\" crossorigin=\"anonymous\"></script>\n    <style>body {\n        padding: 10px;\n    }</style>\n</head>\n<body>\n<div>\n    <canvas id=\"graph\" style=\"width: 100%; height: 300px;\"></canvas>\n</div>\n<div>\n    <p>\n        <label>Temperature</label>: <span id=\"temp\"></span>&nbsp;&nbsp;\n        <label>RPM</label>: <span id=\"rpm\"></span>&nbsp;&nbsp;\n        <label>Duty</label>: <span id=\"duty\"></span>&nbsp;&nbsp;\n        <label>Uptime</label>: <span id=\"uptime\"></span>\n</div>\n<script>\n    const MAX_ENTRIES = 300;\n\n    let chart = new Chart('graph', {\n        data: {\n            datasets: [\n                {\n                    label: 'Temperature',\n                    type: 'line',\n                    fill: false,\n                    pointRadius: 0,\n                    yAxisID: 'temp',\n                    borderColor: '#A03333',\n                    backgroundColor: '#A03333'\n                },\n                {\n                    label: 'Fan',\n                    type: 'line',\n                    fill: false,\n                    pointRadius: 0,\n                    yAxisID: 'rpm',\n                    borderColor: '#009900',\n                    backgroundColor: '#009900'\n                }\n            ]\n        },\n        options: {\n            maintainAspectRatio: false,\n            scales: {\n                xAxes: [{type: 'time', time: {minUnit: 'second'}, bounds: 'ticks'}],\n                yAxes: [{\n                    id: 'temp',\n                    scaleLabel: {\n                        display: true,\n                        labelString: '°C'\n                    },\n                    ticks: {beginAtZero: true}\n                }, {\n                    id: 'rpm',\n                    scaleLabel: {\n                        display: true,\n                        labelString: 'RPM'\n                    },\n                    ticks: {beginAtZero: true},\n                    position: 'right'\n                }]\n            },\n            tooltips: {\n                intersect: false,\n                mode: 'index'\n            }\n        }\n    });\n\n    chart.config.data.datasets[0].data.push({x: Date.now() - MAX_ENTRIES * 1000, y: 0});\n    chart.config.data.datasets[1].data.push({x: Date.now() - MAX_ENTRIES * 1000, y: 0});\n\n    function add(json) {\n        let now = Date.now();\n        console.log(json);\n        chart.config.data.datasets[0].data.push({x: now, y: json.temp || 0});\n        chart.config.data.datasets[1].data.push({x: now, y: json.rpm || 0});\n        chart.update();\n\n        document.getElementById('temp').innerText = json.temp || 0;\n        document.getElementById('duty').innerText = json.duty || 0;\n        document.getElementById('rpm').innerText = json.rpm || 0;\n        document.getElementById('uptime').innerText = ((json.up || 0) / 1000).toFixed(0);\n    }\n\n    function update() {\n        fetch(new Request('/data'))\n            .then(response => {\n                if (response.status === 200) {\n                    return response.json();\n                }\n            })\n            .then(add)\n    }\n\n    setInterval(update, 2000);\n</script>\n</body>\n</html>\n";

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
    // TODO
    //m << "esp_celsius{hardware=\"" << hardware << "\",sensor=\"Water Intake\"} " << std::fixed << std::setprecision(3) << Values::temperature.load() << '\n';

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
    std::ostringstream json;

    json << "{\n  \"version\":\"" << VERSION << "\"\n}";

    auto resp = json.str();
    ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_type(req, "application/json"));
    return httpd_resp_send(req, resp.c_str(), resp.length());
}

// TODO move JSON logic to app_config_json.h

esp_err_t getConfigHandler(httpd_req_t *req)
{
    StaticJsonDocument<CONFIG_JSON_SIZE> json;

    appconfig::app_config config; // TODO

    json["control_pin"] = config.data.control_pin;
    {
        auto rpm_pins = json.createNestedArray("rpm_pins");
        for (size_t i = 0; i < appconfig::APP_CONFIG_MAX_RPM; i++)
        {
            rpm_pins.add(config.data.rpm_pins[i]);
        }
    }
    json["sensors_pin"] = config.data.sensors_pin;
    json["primary_sensor_address"] = config.data.primary_sensor_address;
    {
        auto sensors = json.createNestedArray("sensors");
        for (size_t i = 0; i < appconfig::APP_CONFIG_MAX_SENSORS; i++)
        {
            auto &s = config.data.sensors[i];
            auto sensorObj = sensors.createNestedObject();
            sensorObj["address"] = s.address;
            sensorObj["name"] = s.name;
        }
    }
    json["low_threshold_celsius"] = config.data.low_threshold_celsius;
    json["high_threshold_celsius"] = config.data.high_threshold_celsius;
    json["cpu_threshold_celsius"] = config.data.cpu_threshold_celsius;
    json["cpu_poll_interval_seconds"] = config.data.cpu_poll_interval_seconds;
    json["hardware_name"] = config.data.hardware_name;
    json["cpu_query_url"] = config.cpu_query_url;

    HttpdResponseWriter writer(req);
    ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_type(req, "application/json"));
    serializeJson(json, writer);
    return writer.close();
}

esp_err_t putConfigHandler(httpd_req_t *req)
{
    StaticJsonDocument<CONFIG_JSON_SIZE> json;
    HttpdRequestReader reader(req);
    if (req->content_len == 0 || deserializeJson(json, reader) != DeserializationError::Ok)
    {
        return httpd_resp_set_status(req, "400 Bad Request");
    }

    appconfig::app_config config;

    config.data.control_pin = static_cast<gpio_num_t>(json["control_pin"].as<uint8_t>());
    {
        size_t i = 0;
        for (auto rpmPin : json["rpm_pins"].as<JsonArrayConst>())
        {
            config.data.rpm_pins[i++] = static_cast<gpio_num_t>(rpmPin.as<uint8_t>());
        }
    }
    config.data.sensors_pin = static_cast<gpio_num_t>(json["sensors_pin"].as<uint8_t>());
    config.data.primary_sensor_address = json["primary_sensor_address"].as<uint64_t>();
    {
        size_t i = 0;
        for (auto sensorObj : json["sensors"].as<JsonArrayConst>())
        {
            auto &s = config.data.sensors[i];
            s.address = sensorObj["address"].as<uint64_t>();
            sensorObj["name"].as<std::string>().copy(s.name, appconfig::APP_CONFIG_MAX_NAME_LENGHT - 1);
            i++;
        }
    }
    config.data.low_threshold_celsius = json["low_threshold_celsius"].as<uint8_t>();
    config.data.high_threshold_celsius = json["high_threshold_celsius"].as<uint8_t>();
    config.data.cpu_threshold_celsius = json["cpu_threshold_celsius"].as<uint8_t>();
    config.data.cpu_poll_interval_seconds = json["cpu_poll_interval_seconds"].as<uint16_t>();
    json["hardware_name"].as<std::string>().copy(config.data.hardware_name, appconfig::APP_CONFIG_MAX_NAME_LENGHT - 1);
    config.cpu_query_url = json["cpu_query_url"].as<std::string>();

    // Update
    esp_err_t err = appconfig::app_config_update(config);
    if (err != ESP_OK)
    {
        // Failed
        return err;
    }

    // Success
    return httpd_resp_set_status(req, "204 No Content");
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

void setupHttp(const std::string &hardware, const std::vector<std::string> &sensorNames)
{
    // Store config
    ::hardware = hardware;
    ::sensorNames = sensorNames;

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
