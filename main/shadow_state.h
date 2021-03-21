#pragma once

#include <functional>
#include <nvs_handle.hpp>
#include <rapidjson/pointer.h>
#include <string>
#include <utility>
#include <vector>

struct shadow_state;

struct shadow_state_accessor
{
    virtual ~shadow_state_accessor() = default;

    virtual shadow_state *get_state() noexcept = 0;
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
    virtual bool Get(const rapidjson::Value &root) = 0;
    virtual void Set(rapidjson::Value &root, rapidjson::Value::AllocatorType &allocator) = 0;

    virtual void Load(nvs::NVSHandle &handle) = 0;
    virtual void Store(nvs::NVSHandle &handle) = 0;

    shadow_state *get_state() noexcept override
    {
        return this;
    }
};

struct shadow_state_set : shadow_state
{
    inline void Add(shadow_state &state) noexcept
    {
        states_.push_back(&state);
    }

    bool Get(const rapidjson::Value &root) final
    {
        bool changed = false;
        for (auto state : states_)
        {
            changed |= state->Get(root);
        }
        return changed;
    }

    void Set(rapidjson::Value &root, rapidjson::Value::AllocatorType &allocator) final
    {
        for (auto state : states_)
        {
            state->Set(root, allocator);
        }
    }

    void Load(nvs::NVSHandle &handle) final
    {
        for (auto state : states_)
        {
            state->Load(handle);
        }
    }

    void Store(nvs::NVSHandle &handle) final
    {
        for (auto state : states_)
        {
            state->Store(handle);
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
          key(jsonPointer + 1), // strip leading '/'
          value(value)
    {
    }

    shadow_state_ref(shadow_state_set &set, const char *jsonPointer, T &value)
        : shadow_state_ref(jsonPointer, value)
    {
        set.Add(*this);
    }

    bool Get(const rapidjson::Value &root) final
    {
        return GetValue(root, ptr, value);
    }

    void Set(rapidjson::Value &root, rapidjson::Value::AllocatorType &allocator) final
    {
        ptr.Set<T>(root, value, allocator);
    }

    void Load(nvs::NVSHandle &handle) final
    {
        handle.get_item<T>(key.c_str(), value);
    }

    void Store(nvs::NVSHandle &handle) final
    {
        handle.set_item<T>(key.c_str(), value);
    }

    /**
     * Gets value from given JSON object root and stores it to this instance.
     * Ignores invalid value type.
     *
     * @param root JSON root object
     * @param
     * @return true if value has changed, false otherwise
     */
    static bool GetValue(const rapidjson::Value &root, const rapidjson::Pointer &ptr, T &value)
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
};

template<>
bool shadow_state_ref<std::string>::Get(const rapidjson::Value &root);

template<>
void shadow_state_ref<std::string>::Set(rapidjson::Value &root, rapidjson::Value::AllocatorType &allocator);

template<>
void shadow_state_ref<std::string>::Load(nvs::NVSHandle &handle);

template<>
void shadow_state_ref<std::string>::Store(nvs::NVSHandle &handle);

template<>
void shadow_state_ref<double>::Load(nvs::NVSHandle &handle);

template<>
void shadow_state_ref<double>::Store(nvs::NVSHandle &handle);

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
        set.Add(*this);
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
          key(jsonPointer + 1), // strip leading '/'
          itemFactory(std::move(itemFactory))
    {
    }

    shadow_state_list(shadow_state_set &set, const char *jsonPointer, std::function<T *()> itemFactory)
        : shadow_state_list(jsonPointer, itemFactory)
    {
        set.Add(*this);
    }

    bool Get(const rapidjson::Value &root) final
    {
        const rapidjson::Value *list = ptr.Get(root);
        if (!list || !list->IsArray())
        {
            return false;
        }

        // Resize
        auto array = list->GetArray();

        if (items.size() > array.Size())
        {
            items.resize(array.Size());
        }
        else
        {
            while (items.size() < array.Size())
            {
                items.emplace_back(std::unique_ptr<T>(itemFactory()));
            }
        }

        // Get each
        bool changed = false;
        for (size_t i = 0; i < array.Size(); i++)
        {
            changed |= items[i]->Get(array[i]);
        }
        return changed;
    }

    void Set(rapidjson::Value &root, rapidjson::Value::AllocatorType &allocator) final
    {
        auto &array = ptr.Create(root, allocator);
        if (!array.IsArray())
        {
            array.SetArray();
        }

        // Resize
        array.Reserve(items.size(), allocator);

        while (array.Size() < items.size())
        {
            array.PushBack(rapidjson::Value(), allocator);
        }
        while (array.Size() > items.size())
        {
            array.PopBack();
        }

        // Set each
        for (size_t i = 0; i < array.Size(); i++)
        {
            items[i]->Set(array[i], allocator);
        }
    }

    void Load(nvs::NVSHandle &handle) final
    {
        // TODO
    }

    void Store(nvs::NVSHandle &handle) final
    {
        // TODO
    }
};
