#pragma once

#include "serialization.h"
#include <cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const struct serialization_functions serialization_functions_cjson;

struct serialization_context_cjson
{
    struct serialization_context base;
    cJSON *obj;
};

#define SERIALIZATION_CONTEXT_CJSON_INITIALIZE(obj_)     \
    {                                                    \
        .base = {                                        \
            .functions = &serialization_functions_cjson, \
        },                                               \
        .obj = obj_,                                     \
    }

struct serialization_context *serialization_context_create_cjson(cJSON *obj);

#ifdef __cplusplus
}
#endif
