#include <atomic>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_task_wdt.h>
#include <esp_http_client.h>
#include <cJSON.h>

namespace cputemp
{
    static unsigned long intervalMs_;
    static std::atomic<float> temperature_;

    float cpu_temp_value()
    {
        return temperature_;
    }

    esp_err_t cpu_temp_parse_response(const void *data, size_t len)
    {
        StaticJsonDocument<JSON_OBJECT_SIZE(8) + 26> doc;
        if (deserializeJson(doc, data, len) != DeserializationError::Ok)
        {
            log_e("failed to deserialize JSON response");
            return ESP_FAIL;
        }

        const char *cpuTempStr = doc["data"]["result"][1].as<const char *>();
        if (cpuTempStr)
        {
            float temperature = atof(cpuTempStr);
            log_i("received cpu temperature %f C", temperature);
            temperature_ = temperature;
        }
        else
        {
            log_e("failed to find temperature in the reponse");
            return ESP_FAIL;
        }

        return ESP_OK;
    }

    esp_err_t cpu_temp_event_handler(esp_http_client_event_t *evt)
    {
        switch (evt->event_id)
        {
        case HTTP_EVENT_ERROR:
            log_i("HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client))
            {
                return cpu_temp_parse_response(evt->data, evt->data_len);
            }
            else
            {
                log_e("chunked response not supported");
                return ESP_FAIL;
            }
        default:
            break;
        }
        return ESP_OK;
    }

    void cpu_temp_loop(void *p)
    {
        auto client = static_cast<esp_http_client_handle_t>(p);

        // Enable Watchdog
        // ESP_ERROR_CHECK_WITHOUT_ABORT(esp_task_wdt_add(nullptr));

        TickType_t previousWakeTime = xTaskGetTickCount();

        while (true)
        {
            // Watchdog
            // ESP_ERROR_CHECK_WITHOUT_ABORT(esp_task_wdt_reset());

            // Request
            ESP_ERROR_CHECK_WITHOUT_ABORT(esp_http_client_perform(client));

            // Wait
            vTaskDelayUntil(&previousWakeTime, intervalMs_ / portTICK_PERIOD_MS);
        }
    }

    esp_err_t cpu_temp_init(const std::string &url, unsigned long intervalMs)
    {
        log_i("initializing with url %s and interval %d ms", url.c_str(), intervalMs);

        if (url.rfind("http", 0) != 0)
        {
            return ESP_FAIL;
        }

        // Store
        intervalMs_ = std::max(500ul, intervalMs);

        // Config
        auto config = esp_http_client_config_t{
            .url = url.c_str(),
        };
        config.timeout_ms = std::min(1000ul, intervalMs_ - 100);
        config.event_handler = cpu_temp_event_handler;
        config.buffer_size = 512;

        // Init
        esp_http_client_handle_t client = esp_http_client_init(&config);

        // Start loop
        xTaskCreate(cpu_temp_loop, "cpu_temp", 8192, client, tskIDLE_PRIORITY, nullptr);
        return ESP_OK;
    }

} // namespace cputemp
