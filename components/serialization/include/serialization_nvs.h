#pragma once

#include "serialization.h"
#include <nvs.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const struct serialization_functions serialization_functions_nvs;

struct serialization_context_nvs
{
    struct serialization_context base;
    nvs_handle_t handle;
    char prefix[16]; // nvs has limit max 15 chars atm
    int index;
};

#define SERIALIZATION_CONTEXT_NVS_INITIALIZE(handle_)    \
    {                                                    \
        .base = {                                        \
            .functions = &serialization_functions_cjson, \
        },                                               \
        .handle = handle_, .prefix = {}, .index = -1,    \
    }

struct serialization_context *serialization_context_create_nvs(nvs_handle_t handle);

#ifdef __cplusplus
}
#endif
