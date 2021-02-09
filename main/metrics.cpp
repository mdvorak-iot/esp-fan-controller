#include "metrics.h"
#include <sstream>
#include <iomanip>
#include <vector>
#include <functional>

static const char TAG[] = "metrics";

static std::vector<metrics_callback> metric_providers;

void metrics_register_callback(const metrics_callback &cb)
{
    metric_providers.push_back(cb);
}

esp_err_t metrics_http_handler(httpd_req_t *req)
{
    // NOTE it is faster to write this in memory, then sending it in chunks
    std::ostringstream out;

    for (const auto &p : metric_providers)
    {
        p(out);
    }

    // Send response
    std::string body = out.str();
    ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_type(req, "text/plain"));
    return httpd_resp_send(req, body.c_str(), body.length());
}
