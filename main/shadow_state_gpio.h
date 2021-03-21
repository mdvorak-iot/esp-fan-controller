#pragma once

#include "shadow_state.h"
#include <hal/gpio_types.h>

template<>
bool ShadowState<gpio_num_t>::Get(const rapidjson::Value &root);

template<>
void ShadowState<gpio_num_t>::Load(nvs::NVSHandle &handle);

template<>
void ShadowState<gpio_num_t>::Store(nvs::NVSHandle &handle);
