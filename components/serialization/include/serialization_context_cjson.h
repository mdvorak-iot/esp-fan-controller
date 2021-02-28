#pragma once

#include <cJSON.h>

struct serialization_context_cjson;

#ifdef __cplusplus
extern "C" {
#endif

struct serialization_context_cjson *serialization_context_cjson_create(cJSON *obj);

#ifdef __cplusplus
}
#endif
