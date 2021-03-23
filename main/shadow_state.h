#pragma once

#include <cstring>
#include <esp_log.h>
#include <functional>
#include <nvs_handle.hpp>
#include <rapidjson/pointer.h>
#include <string>
#include <vector>

// TODO move to cpp, rename
inline static std::string nvs_key(const std::string &s)
{
    return !s.empty() && s[0] == '/' ? s.substr(1, std::string::npos) : s; // Skip leading '/' char
}

static const char *nvs_key(const char *s)
{
    assert(s);
    return *s == '/' ? s + 1 : s; // Skip leading '/' char
}

template<typename S>
struct shadow_state
{
    virtual ~shadow_state() = default;

    /**
     * Gets value from given JSON object root and stores it to this instance.
     * Ignores invalid value type.
     *
     * @param root JSON root object
     * @return true if value has changed, false otherwise
     */
    virtual bool get(const rapidjson::Value &root, S &inst) const = 0;
    virtual void set(rapidjson::Value &root, rapidjson::Value::AllocatorType &allocator, const S &inst) const = 0;

    // TODO propagate error
    void load(nvs::NVSHandle &handle, S &inst) const
    {
        load(handle, nullptr, inst);
    }

    // TODO propagate error
    void store(nvs::NVSHandle &handle, const S &inst) const
    {
        store(handle, nullptr, inst);
    }

    virtual void load(nvs::NVSHandle &handle, const char *prefix, S &inst) const = 0;
    virtual void store(nvs::NVSHandle &handle, const char *prefix, const S &inst) const = 0;
};

template<typename S>
struct shadow_state_set : shadow_state<S>
{
    shadow_state_set() = default;
    shadow_state_set(const shadow_state_set &) = delete;

    ~shadow_state_set() override
    {
        for (auto state : states_)
        {
            delete state;
        }
    }

    inline void add(const shadow_state<S> *state) noexcept
    {
        assert(state);
        states_.push_back(state);
    }

    bool get(const rapidjson::Value &root, S &inst) const final
    {
        bool changed = false;
        for (auto state : states_)
        {
            changed |= state->get(root, inst);
        }
        return changed;
    }

    void set(rapidjson::Value &root, rapidjson::Value::AllocatorType &allocator, const S &inst) const final
    {
        for (auto state : states_)
        {
            state->set(root, allocator, inst);
        }
    }

    void load(nvs::NVSHandle &handle, const char *prefix, S &inst) const final
    {
        for (auto state : states_)
        {
            state->load(handle, prefix, inst);
        }
    }

    void store(nvs::NVSHandle &handle, const char *prefix, const S &inst) const final
    {
        for (auto state : states_)
        {
            state->store(handle, prefix, inst);
        }
    }

 private:
    std::vector<const shadow_state<S> *> states_;
};

template<typename T>
struct shadow_state_helper
{
    /**
     * Gets value from given JSON object root and stores it to this instance.
     * Ignores invalid value type.
     *
     * @param root JSON root object
     * @param ptr JSON pointer
     * @param value Value reference
     * @return true if value has changed, false otherwise
     */
    static bool get(const rapidjson::Pointer &ptr, const rapidjson::Value &root, T &value)
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

    static void set(const rapidjson::Pointer &ptr, rapidjson::Value &root, rapidjson::Value::AllocatorType &allocator, const T &value)
    {
        ptr.Set<T>(root, value, allocator);
    }

    static void load(const std::string &key, nvs::NVSHandle &handle, const char *prefix, T &value)
    {
        const std::string full_key = nvs_key(prefix && prefix[0] != '\0' ? prefix + key : key);
        esp_err_t err = handle.get_item<T>(full_key.c_str(), value);
        if (err != ESP_OK)
        {
            ESP_LOGW("shadow_state", "failed to get_item %s: %d %s", full_key.c_str(), err, esp_err_to_name(err));
        }
    }

    static void store(const std::string &key, nvs::NVSHandle &handle, const char *prefix, const T &value)
    {
        const std::string full_key = nvs_key(prefix && prefix[0] != '\0' ? prefix + key : key);
        esp_err_t err = handle.set_item<T>(full_key.c_str(), value);
        if (err != ESP_OK)
        {
            ESP_LOGW("shadow_state", "failed to set_item %s: %d %s", full_key.c_str(), err, esp_err_to_name(err));
        }
    }
};

template<>
bool shadow_state_helper<std::string>::get(const rapidjson::Pointer &ptr, const rapidjson::Value &root, std::string &value);

template<>
void shadow_state_helper<std::string>::set(const rapidjson::Pointer &ptr, rapidjson::Value &root, rapidjson::Value::AllocatorType &allocator, const std::string &value);

template<>
void shadow_state_helper<std::string>::load(const std::string &key, nvs::NVSHandle &handle, const char *prefix, std::string &value);

template<>
void shadow_state_helper<std::string>::store(const std::string &key, nvs::NVSHandle &handle, const char *prefix, const std::string &value);

template<>
void shadow_state_helper<float>::load(const std::string &key, nvs::NVSHandle &handle, const char *prefix, float &value);

template<>
void shadow_state_helper<float>::store(const std::string &key, nvs::NVSHandle &handle, const char *prefix, const float &value);

template<>
void shadow_state_helper<double>::load(const std::string &key, nvs::NVSHandle &handle, const char *prefix, double &value);

template<>
void shadow_state_helper<double>::store(const std::string &key, nvs::NVSHandle &handle, const char *prefix, const double &value);

template<typename S, typename T>
struct shadow_state_field : shadow_state<S>
{
    const rapidjson::Pointer ptr;
    const std::string key;
    T S::*const field;

    shadow_state_field(const char *json_ptr, T S::*field)
        : ptr(json_ptr),
          key(json_ptr),
          field(field)
    {
        assert(field);
    }

    bool get(const rapidjson::Value &root, S &inst) const final
    {
        return shadow_state_helper<T>::get(ptr, root, inst.*field);
    }

    void set(rapidjson::Value &root, rapidjson::Value::AllocatorType &allocator, const S &inst) const final
    {
        shadow_state_helper<T>::set(ptr, root, allocator, inst.*field);
    }

    void load(nvs::NVSHandle &handle, const char *prefix, S &inst) const final
    {
        shadow_state_helper<T>::load(key, handle, prefix, inst.*field);
    }

    void store(nvs::NVSHandle &handle, const char *prefix, const S &inst) const final
    {
        shadow_state_helper<T>::store(key, handle, prefix, inst.*field);
    }
};

template<typename T>
struct shadow_state_value : shadow_state<T>
{
    const rapidjson::Pointer ptr;
    const std::string key;

    explicit shadow_state_value(const char *json_ptr)
        : ptr(json_ptr),
          key(json_ptr)
    {
    }

    bool get(const rapidjson::Value &root, T &inst) const final
    {
        return shadow_state_helper<T>::get(ptr, root, inst);
    }

    void set(rapidjson::Value &root, rapidjson::Value::AllocatorType &allocator, const T &inst) const final
    {
        shadow_state_helper<T>::set(ptr, root, allocator, inst);
    }

    void load(nvs::NVSHandle &handle, const char *prefix, T &inst) const final
    {
        shadow_state_helper<T>::load(key, handle, prefix, inst);
    }

    void store(nvs::NVSHandle &handle, const char *prefix, const T &inst) const final
    {
        shadow_state_helper<T>::store(key, handle, prefix, inst);
    }
};

template<typename S, typename T>
struct shadow_state_list : shadow_state<S>
{
    const rapidjson::Pointer ptr;
    const std::string key;
    std::vector<T> S::*const field;
    const std::unique_ptr<const shadow_state<T>> element;

    shadow_state_list(const char *json_ptr, std::vector<T> S::*field, const shadow_state<T> *element)
        : ptr(json_ptr),
          key(json_ptr),
          field(field),
          element(element)
    {
        assert(field);
        assert(element);
    }

    bool get(const rapidjson::Value &root, S &inst) const final
    {
        const rapidjson::Value *list = ptr.Get(root);
        if (!list || !list->IsArray())
        {
            return false;
        }

        auto &items = inst.*field;

        // Resize
        auto array = list->GetArray();
        size_t length = array.Size();

        items.resize(length);

        // Get each
        bool changed = false;
        for (size_t i = 0; i < length; i++)
        {
            changed |= element->get(array[i], items[i]);
        }
        return changed;
    }

    void set(rapidjson::Value &root, rapidjson::Value::AllocatorType &allocator, const S &inst) const final
    {
        auto &array = ptr.Create(root, allocator);
        if (!array.IsArray())
        {
            array.SetArray();
        }

        auto &items = inst.*field;
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
            element->set(array[i], allocator, items[i]);
        }
    }

    void load(nvs::NVSHandle &handle, const char *prefix, S &inst) const final
    {
        char item_prefix[16] = {};
        if (!prefix) prefix = "";

        auto &items = inst.*field;

        // Read length
        std::snprintf(item_prefix, sizeof(item_prefix) - 1, "%s%s/len", prefix, key.c_str());
        uint16_t length = 0;
        handle.get_item(nvs_key(item_prefix), length); // Ignore error

        // Resize
        items.resize(length);

        // Read items
        for (size_t i = 0; i < items.size(); i++)
        {
            std::snprintf(item_prefix, sizeof(item_prefix) - 1, "%s%s/%uz", prefix, key.c_str(), i);
            element->load(handle, nvs_key(item_prefix), items[i]);
        }
    }

    void store(nvs::NVSHandle &handle, const char *prefix, const S &inst) const final
    {
        char item_prefix[16] = {};
        if (!prefix) prefix = "";

        auto &items = inst.*field;

        // Store length
        std::snprintf(item_prefix, sizeof(item_prefix) - 1, "%s%s/len", prefix, key.c_str());
        handle.set_item(nvs_key(item_prefix), static_cast<uint16_t>(items.size())); // No need to store all 32 bytes, that would never fit in memory

        // Store items
        for (size_t i = 0; i < items.size(); i++)
        {
            std::snprintf(item_prefix, sizeof(item_prefix) - 1, "%s%s/%uz", prefix, key.c_str(), i);
            element->store(handle, nvs_key(item_prefix), items[i]);
        }
    }
};
