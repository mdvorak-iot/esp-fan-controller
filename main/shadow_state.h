#pragma once

#include <cstring>
#include <functional>
#include <nvs_handle.hpp>
#include <rapidjson/pointer.h>
#include <string>
#include <vector>

struct shadow_state;

struct shadow_state_accessor
{
    virtual ~shadow_state_accessor() = default;

    virtual shadow_state *get_shadow_state() noexcept = 0;
};

struct shadow_state : shadow_state_accessor
{
    /**
     * Gets value from given JSON object root and stores it to this instance.
     * Ignores invalid value type.
     *
     * @param root JSON root object
     * @return true if value has changed, false otherwise
     */
    virtual bool get(const rapidjson::Value &root) = 0;
    virtual void set(rapidjson::Value &root, rapidjson::Value::AllocatorType &allocator) = 0;

    virtual void load(nvs::NVSHandle &handle, const char *prefix) = 0;
    virtual void store(nvs::NVSHandle &handle, const char *prefix) = 0;

    shadow_state *get_shadow_state() noexcept override
    {
        return this;
    }

 protected:
    static const char *nvs_key(const std::string &s)
    {
        return s.empty() ? "" : &s[1]; // Skip leading '/' char
    }

    static const char *nvs_key(const char *s)
    {
        assert(s);
        return *s != '\0' ? s + 1 : s; // Skip leading '/' char
    }
};

struct shadow_state_set : shadow_state
{
    inline void add(shadow_state &state) noexcept
    {
        states_.push_back(&state);
    }

    bool get(const rapidjson::Value &root) final
    {
        bool changed = false;
        for (auto state : states_)
        {
            changed |= state->get(root);
        }
        return changed;
    }

    void set(rapidjson::Value &root, rapidjson::Value::AllocatorType &allocator) final
    {
        for (auto state : states_)
        {
            state->set(root, allocator);
        }
    }

    void load(nvs::NVSHandle &handle, const char *prefix) final
    {
        for (auto state : states_)
        {
            state->load(handle, prefix);
        }
    }

    void store(nvs::NVSHandle &handle, const char *prefix) final
    {
        for (auto state : states_)
        {
            state->store(handle, prefix);
        }
    }

 private:
    std::vector<shadow_state *> states_;
};

template<typename T>
struct shadow_state_ref : shadow_state
{
    const rapidjson::Pointer ptr;
    const std::string key;
    T &value;

    shadow_state_ref(const char *jsonPointer, T &value)
        : ptr(jsonPointer),
          key(jsonPointer),
          value(value)
    {
    }

    shadow_state_ref(shadow_state_set &set, const char *jsonPointer, T &value)
        : shadow_state_ref(jsonPointer, value)
    {
        set.add(*this);
    }

    bool get(const rapidjson::Value &root) final
    {
        return get_value(root, ptr, value);
    }

    void set(rapidjson::Value &root, rapidjson::Value::AllocatorType &allocator) final
    {
        ptr.Set<T>(root, value, allocator);
    }

    void load(nvs::NVSHandle &handle, const char *prefix) final
    {
        handle.get_item<T>(nvs_key(prefix && prefix[0] != '\0' ? prefix + key : key), value);
    }

    void store(nvs::NVSHandle &handle, const char *prefix) final
    {
        handle.set_item<T>(nvs_key(prefix && prefix[0] != '\0' ? prefix + key : key), value);
    }

    /**
     * Gets value from given JSON object root and stores it to this instance.
     * Ignores invalid value type.
     *
     * @param root JSON root object
     * @param
     * @return true if value has changed, false otherwise
     */
    static bool get_value(const rapidjson::Value &root, const rapidjson::Pointer &ptr, T &value)
    {
        // Find object
        const rapidjson::Value *obj = ptr.Get(root);
        // Check its type
        if (obj && obj->Is<T>())
        {
            // Get new value
            T new_value = obj->Get<T>();
            if (new_value != value)
            {
                // If it is different, update
                value = new_value;
                return true;
            }
        }
        return false;
    }
};

template<>
bool shadow_state_ref<std::string>::get(const rapidjson::Value &root);

template<>
void shadow_state_ref<std::string>::set(rapidjson::Value &root, rapidjson::Value::AllocatorType &allocator);

template<>
void shadow_state_ref<std::string>::load(nvs::NVSHandle &handle, const char *prefix);

template<>
void shadow_state_ref<std::string>::store(nvs::NVSHandle &handle, const char *prefix);

template<>
void shadow_state_ref<float>::load(nvs::NVSHandle &handle, const char *prefix);

template<>
void shadow_state_ref<float>::store(nvs::NVSHandle &handle, const char *prefix);

template<>
void shadow_state_ref<double>::load(nvs::NVSHandle &handle, const char *prefix);

template<>
void shadow_state_ref<double>::store(nvs::NVSHandle &handle, const char *prefix);

template<typename T>
struct shadow_state_value : shadow_state_ref<T>
{
    shadow_state_value(const char *jsonPointer, T defaultValue)
        : shadow_state_ref<T>(jsonPointer, valueHolder),
          valueHolder(defaultValue)
    {
    }

    shadow_state_value(shadow_state_set &set, const char *jsonPointer, T defaultValue)
        : shadow_state_value(jsonPointer, defaultValue)
    {
        set.add(*this);
    }

 protected:
    T valueHolder;
};

template<typename T = shadow_state>
struct shadow_state_list : shadow_state
{
    const rapidjson::Pointer ptr;
    const std::string key;
    const std::function<T *()> itemFactory;
    std::vector<std::unique_ptr<T>> items;

    shadow_state_list(const char *jsonPointer, std::function<T *()> itemFactory)
        : ptr(jsonPointer),
          key(jsonPointer),
          itemFactory(std::move(itemFactory))
    {
    }

    shadow_state_list(shadow_state_set &set, const char *jsonPointer, std::function<T *()> itemFactory)
        : shadow_state_list(jsonPointer, itemFactory)
    {
        set.add(*this);
    }

    bool get(const rapidjson::Value &root) final
    {
        const rapidjson::Value *list = ptr.Get(root);
        if (!list || !list->IsArray())
        {
            return false;
        }

        // Resize
        auto array = list->GetArray();
        size_t len = array.Size();

        resize(len);

        // Get each
        bool changed = false;
        for (size_t i = 0; i < array.Size(); i++)
        {
            changed |= items[i]->get_shadow_state()->get(array[i]);
        }
        return changed;
    }

    void set(rapidjson::Value &root, rapidjson::Value::AllocatorType &allocator) final
    {
        auto &array = ptr.Create(root, allocator);
        if (!array.IsArray())
        {
            array.SetArray();
        }

        size_t len = items.size();

        // Resize array
        array.Reserve(len, allocator);

        while (array.Size() < len)
        {
            array.PushBack(rapidjson::Value(), allocator);
        }
        while (array.Size() > len)
        {
            array.PopBack();
        }

        // Set each
        for (size_t i = 0; i < len; i++)
        {
            items[i]->get_shadow_state()->set(array[i], allocator);
        }
    }

    void load(nvs::NVSHandle &handle, const char *prefix) final
    {
        char item_prefix[16] = {};
        if (!prefix) prefix = "";

        // Read length
        std::snprintf(item_prefix, sizeof(item_prefix) - 1, "%s%s/len", prefix, key.c_str());
        uint16_t length = 0;
        handle.get_item(nvs_key(item_prefix), length); // Ignore error

        // Resize
        resize(length);

        // Read items
        for (size_t i = 0; i < items.size(); i++)
        {
            std::snprintf(item_prefix, sizeof(item_prefix) - 1, "%s%s/%uz", prefix, key.c_str(), i);
            items[i]->get_shadow_state()->load(handle, nvs_key(item_prefix));
        }
    }

    void store(nvs::NVSHandle &handle, const char *prefix) final
    {
        char item_prefix[16] = {};
        if (!prefix) prefix = "";

        // Store length
        std::snprintf(item_prefix, sizeof(item_prefix) - 1, "%s%s/len", prefix, key.c_str());
        handle.set_item(nvs_key(item_prefix), static_cast<uint16_t>(items.size())); // No need to store all 32 bytes, that would never fit in memory

        // Store items
        for (size_t i = 0; i < items.size(); i++)
        {
            std::snprintf(item_prefix, sizeof(item_prefix) - 1, "%s%s/%uz", prefix, key.c_str(), i);
            items[i]->get_shadow_state()->store(handle, nvs_key(item_prefix));
        }
    }

    void resize(size_t new_size)
    {
        if (items.size() > new_size)
        {
            items.resize(new_size);
        }
        else
        {
            items.reserve(new_size);
            while (items.size() < new_size)
            {
                items.emplace_back(std::unique_ptr<T>(itemFactory()));
            }
        }
    }
};
