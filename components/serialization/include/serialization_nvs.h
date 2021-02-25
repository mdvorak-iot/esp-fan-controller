#pragma once

#include "serialization.h"
#include <nvs.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const struct serialization_functions serialization_functions_nvs;

struct serialization_context *serialization_context_create_nvs(nvs_handle_t handle);

#ifdef __cplusplus
}
#endif
