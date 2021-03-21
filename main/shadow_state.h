#pragma once

#include <new>

#include <nvs_handle.hpp>
#include <rapidjson/pointer.h>
#include <string>
#include <vector>

struct ShadowStateAccessor
{
    virtual bool Get(const rapidjson::Value &root) = 0;
    virtual void Set(rapidjson::Value &root, rapidjson::Value::AllocatorType &allocator) = 0;

    virtual void Load(nvs::NVSHandle &handle) = 0;
    virtual void Store(nvs::NVSHandle &handle) = 0;
};

struct ShadowStateSet
{
    inline void Add(ShadowStateAccessor &state)
    {
        states_.push_back(&state);
    }

    bool Get(const rapidjson::Value &root)
    {
        bool changed = false;
        for (auto state : states_)
        {
            changed |= state->Get(root);
        }
        return changed;
    }

    void Set(rapidjson::Value &root, rapidjson::Value::AllocatorType &allocator)
    {
        for (auto state : states_)
        {
            state->Set(root, allocator);
        }
    }

    void Load(nvs::NVSHandle &handle)
    {
        for (auto state : states_)
        {
            state->Load(handle);
        }
    }

    void Store(nvs::NVSHandle &handle)
    {
        for (auto state : states_)
        {
            state->Store(handle);
        }
    }

 private:
    std::vector<ShadowStateAccessor *> states_;
};

template<typename T>
struct ShadowState : ShadowStateAccessor
{
    rapidjson::Pointer ptr;
    std::string key;
    T value;

    ShadowState(const char *jsonPointer, T defaultValue)
        : ptr(jsonPointer),
          key(jsonPointer + 1), // strip leading '/'
          value(defaultValue)
    {
    }

    ShadowState(ShadowStateSet &set, const char *jsonPointer, T defaultValue)
        : ShadowState(jsonPointer, defaultValue)
    {
        set.Add(*this);
    }

    /**
     * Gets value from given JSON object root and stores it to this instance.
     * Ignores invalid value type.
     *
     * @param root JSON root object
     * @return true if value has changed, false otherwise
     */
    bool Get(const rapidjson::Value &root) final
    {
        // Find object
        const rapidjson::Value *obj = ptr.Get(root);
        // Check its type
        if (obj && obj->Is<T>())
        {
            // Get new value
            T newValue = obj->Get<T>();
            if (newValue != value)
            {
                // If it is different, update
                value = newValue;
                return true;
            }
        }
        return false;
    }

    void Set(rapidjson::Value &root, rapidjson::Value::AllocatorType &allocator) final
    {
        ptr.Set<T>(root, value, allocator);
    }

    void Load(nvs::NVSHandle &handle) override
    {
        handle.get_item<T>(key.c_str(), value);
    }

    void Store(nvs::NVSHandle &handle) override
    {
        handle.set_item<T>(key.c_str(), value);
    }
};

template<>
bool ShadowState<std::string>::Get(const rapidjson::Value &root);

template<>
void ShadowState<std::string>::Set(rapidjson::Value &root, rapidjson::Value::AllocatorType &allocator);

template<>
void ShadowState<std::string>::Load(nvs::NVSHandle &handle);

template<>
void ShadowState<std::string>::Store(nvs::NVSHandle &handle);
