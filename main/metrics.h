#pragma once
#include <esp_http_server.h>
#include <functional>
#include <ostream>
#include <iomanip>

const char METRIC_TYPE_COUNTER[] = "counter";
const char METRIC_TYPE_GAUGE[] = "gauge";

using metrics_callback = std::function<void(std::ostream &)>;

void metrics_register_callback(const metrics_callback &cb);

esp_err_t metrics_http_handler(httpd_req_t *req);

inline std::ostream &metric_type(std::ostream &out, const char *name, const char *type)
{
    return out << "# TYPE " << name << ' ' << type << '\n';
}

class metric_value
{
public:
    metric_value(std::ostream &out, const char *name)
        : out_(out), has_label_(false)
    {
        out << name;
    }

    // NOTE labels should be sorted by name, alphabetically

    inline metric_value &label(const char *name, const char *value)
    {
        begin_label(name) << value << '"';
        return *this;
    }

    inline metric_value &label(const char *name, const std::string &value)
    {
        begin_label(name) << value << '"';
        return *this;
    }

    inline metric_value &label(const char *name, int value)
    {
        begin_label(name) << value << '"';
        return *this;
    }

    inline metric_value &label(const char *name, int64_t value)
    {
        begin_label(name) << value << '"';
        return *this;
    }

    inline metric_value &label(const char *name, uint64_t value)
    {
        begin_label(name) << value << '"';
        return *this;
    }

    inline metric_value &label_hex(const char *name, uint8_t value)
    {
        begin_label(name) << std::setfill('0') << std::setw(2) << std::hex << std::right << value << '"';
        return *this;
    }

    inline metric_value &label_hex(const char *name, uint16_t value)
    {
        begin_label(name) << std::setfill('0') << std::setw(4) << std::hex << std::right << value << '"';
        return *this;
    }

    inline metric_value &label_hex(const char *name, uint32_t value)
    {
        begin_label(name) << std::setfill('0') << std::setw(8) << std::hex << std::right << value << '"';
        return *this;
    }

    inline metric_value &label_hex(const char *name, uint64_t value)
    {
        begin_label(name) << std::setfill('0') << std::setw(16) << std::hex << std::right << value << '"';
        return *this;
    }

    inline std::ostream &is()
    {
        if (has_label_)
        {
            out_ << '}';
        }
        return out_ << ' ';
    }

private:
    std::ostream &out_;
    bool has_label_;

    inline std::ostream &begin_label(const char *name)
    {
        if (has_label_)
        {
            out_ << ',';
        }
        else
        {
            out_ << '{';
            has_label_ = true;
        }
        out_ << name << "=\"";
        return out_;
    }
};
