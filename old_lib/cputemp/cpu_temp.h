#include <esp_err.h>
#include <string>

namespace cputemp
{

    float cpu_temp_value();

    esp_err_t cpu_temp_init(const std::string &url, unsigned long intervalMs);

} // namespace cputemp
