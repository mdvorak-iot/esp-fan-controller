#pragma once

#include "shadow_state.h"
#include <hal/gpio_types.h>

template<>
bool shadow_state_ref<gpio_num_t>::get(const rapidjson::Value &root);

template<>
void shadow_state_ref<gpio_num_t>::load(nvs::NVSHandle &handle, const char *prefix);

template<>
void shadow_state_ref<gpio_num_t>::store(nvs::NVSHandle &handle, const char *prefix);
