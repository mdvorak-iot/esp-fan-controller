#include "app_config.h"
#include <esp32-hal-log.h>

namespace appconfig
{

const auto APP_CONFIG_DATA = "data";
const auto APP_CONFIG_CPU_QUERY_URL = "cpu_query_url";

std::string app_config_cpu_query_url(nvs_handle handle)
{
    char v[1024]{0};
    size_t s = sizeof(v);
    if (nvs_get_str(handle, APP_CONFIG_CPU_QUERY_URL, v, &s) == ESP_OK)
    {
        return std::string(v, s);
    }
    return std::string();
}

esp_err_t app_config_init(app_config &cfg, const char *name)
{
    // Open
    if (cfg.handle != 0)
    {
        auto err = nvs_open(name, NVS_READWRITE, &cfg.handle);
        if (err != ESP_OK)
        {
            return err;
        }
    }

    // Read (ignore errors here)
    size_t size = sizeof(app_config_data);
    nvs_get_blob(cfg.handle, APP_CONFIG_DATA, &cfg.data, &size);

    if (cfg.data.magic_byte != APP_CONFIG_MAGIC_BYTE)
    {
        log_i("app_config magic byte mismatch, using defaults");
        cfg.data = APP_CONFIG_DATA_DEFAULT();
    }

    cfg.cpu_query_url = app_config_cpu_query_url(cfg.handle);

    return ESP_OK;
}

esp_err_t app_config_update(const app_config &cfg)
{
    auto err = nvs_set_blob(cfg.handle, APP_CONFIG_DATA, &cfg.data, sizeof(app_config_data));
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_set_str(cfg.handle, APP_CONFIG_CPU_QUERY_URL, cfg.cpu_query_url.c_str());
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_commit(cfg.handle);
    if (err != ESP_OK)
    {
        return err;
    }

    return ESP_OK;
}

}; // namespace appconfig
