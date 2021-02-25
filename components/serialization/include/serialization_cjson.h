#pragma once

#include "serialization.h"
#include <cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const struct serialization_functions serialization_functions_cjson;

struct serialization_context *serialization_context_create_cjson(cJSON *obj);

#ifdef __cplusplus
}
#endif
