#include "app_config.h"
#include <esp32-hal-log.h>

namespace appconfig
{

static nvs_handle handle_ = 0;

std::string app_config_cpu_query_url()
{
    char v[1024]{0};
    size_t s = sizeof(v);
    if (nvs_get_str(handle_, APP_CONFIG_NVS_CPU_QUERY_URL, v, &s) == ESP_OK)
    {
        log_i("stored cpu_query_url %s", v);
        return std::string(v, s);
    }
    log_i("empty cpu_query_url");
    return std::string();
}

esp_err_t app_config_init(app_config &cfg)
{
    // Open
    if (handle_ == 0)
    {
        auto err = nvs_open(APP_CONFIG_NVS_NAME, NVS_READWRITE, &handle_);
        if (err != ESP_OK)
        {
            return err;
        }
    }

    // Read (ignore errors here)
    size_t size = sizeof(app_config_data);
    nvs_get_blob(handle_, APP_CONFIG_NVS_DATA, &cfg.data, &size);

    if (cfg.data.magic_byte != APP_CONFIG_MAGIC_BYTE)
    {
        log_i("app_config magic byte mismatch, using defaults");
        cfg.data = APP_CONFIG_DATA_DEFAULT();
    }

    cfg.cpu_query_url = app_config_cpu_query_url();

    return ESP_OK;
}

esp_err_t app_config_update(const app_config &cfg)
{
    log_i("app_config update initiated");
    
    assert(handle_ != 0);
    auto err = nvs_set_blob(handle_, APP_CONFIG_NVS_DATA, &cfg.data, sizeof(app_config_data));
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_set_str(handle_, APP_CONFIG_NVS_CPU_QUERY_URL, cfg.cpu_query_url.c_str());
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_commit(handle_);
    if (err != ESP_OK)
    {
        return err;
    }

    log_i("app_config update committed");
    return ESP_OK;
}

}; // namespace appconfig
