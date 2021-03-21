#pragma once

#include "shadow_state.h"
#include <hal/gpio_types.h>

template<>
bool shadow_state_ref<gpio_num_t>::Get(const rapidjson::Value &root);

template<>
void shadow_state_ref<gpio_num_t>::Load(nvs::NVSHandle &handle);

template<>
void shadow_state_ref<gpio_num_t>::Store(nvs::NVSHandle &handle);
