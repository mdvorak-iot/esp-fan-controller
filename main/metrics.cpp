#include <esp_http_server.h>
#include <sstream>
#include <iomanip>

static const char TAG[] = "metrics";

static const char METRIC_TYPE_COUNTER[] = "counter";
static const char METRIC_TYPE_GAUGE[] = "gauge";

static const char METRIC_NAME_ESP_CELSIUS[] = "esp_celsius";

inline static void metrics_type(std::ostringstream &out, const char *name, const char *type)
{
    out << "# TYPE " << name << ' ' << type << '\n';
}

inline static std::ostringstream &metrics_begin_value(std::ostringstream &out, const char *name, const char *hardware, const char *sensor, uint64_t address)
{
    out << name << "{hardware=\"" << hardware << "\",sensor=\"" << sensor << '\"';

    if (address)
    {
        out << ",address=\"" << std::setfill('0') << std::setw(16) << std::uppercase << std::hex << std::right << address << '\"';
    }

    out << '}';
    return out;
}

esp_err_t metrics_get(httpd_req_t *req)
{
    // NOTE it is faster to write this in memory, then sending it in chunks
    std::ostringstream out;

    metrics_type(out, METRIC_NAME_ESP_CELSIUS, METRIC_TYPE_GAUGE);
    metrics_begin_value(out, METRIC_NAME_ESP_CELSIUS, "TODOhw", "TODOsens", 0x00345678abcd1234) << std::fixed << std::setprecision(3) << 33.0345 << '\n';

    /*

    m << "# TYPE esp_celsius gauge\n";
    m << "esp_celsius{hardware=\"" << hardware_ << "\",sensor=\"ESP32\"} " << std::fixed << std::setprecision(3) << temperatureRead() << '\n';

    std::vector<apptemps::temperature_sensor> temps;
    apptemps::temperature_values(temps);
    char addrStr[17]{0};
    for (auto t : temps)
    {
        snprintf(addrStr, 17, "%08X%08X", (uint32_t)(t.address >> 32), (uint32_t)t.address);
        m << "esp_celsius{hardware=\"" << hardware_ << "\",sensor=\"" << t.name << "\",address=\"" << addrStr << "\"} " << std::fixed << std::setprecision(3) << t.value << '\n';
    }

    m << "# TYPE esp_errors counter\n";
    for (auto t : temps)
    {
        if (t.errors > 0)
        {
            m << "esp_errors{hardware=\"" << hardware_ << "\",sensor=\"" << t.name << "\",address=\"" << addrStr << "\"} " << t.errors << '\n';
        }
    }

    // Get RPMs
    m << "# TYPE esp_rpm gauge\n";
    std::vector<uint16_t> rpms;
    rpmcounter::Rpm.values(rpms);
    for (size_t i = 0; i < rpms.size(); i++)
    {
        m << "esp_rpm{hardware=\"" << hardware_ << "\",sensor=\"Fan " << (i + 1) << "\"} " << rpms[i] << '\n';
    }
    */

    // Send response
    std::string body = out.str();
    ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_type(req, "text/plain"));
    return httpd_resp_send(req, body.c_str(), body.length());
}
