#pragma once

#include <functional>
#include <nvs_handle.hpp>
#include <rapidjson/pointer.h>
#include <string>
#include <utility>
#include <vector>

struct ShadowStateAccessor
{
    virtual ~ShadowStateAccessor() = default;

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
    const rapidjson::Pointer ptr;
    const std::string key;
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
bool ShadowState<std::string>::Get(const rapidjson::Value &root);

template<>
void ShadowState<std::string>::Set(rapidjson::Value &root, rapidjson::Value::AllocatorType &allocator);

template<>
void ShadowState<std::string>::Load(nvs::NVSHandle &handle);

template<>
void ShadowState<std::string>::Store(nvs::NVSHandle &handle);

template<>
void ShadowState<double>::Load(nvs::NVSHandle &handle);

template<>
void ShadowState<double>::Store(nvs::NVSHandle &handle);

template<typename T = ShadowStateAccessor>
struct ShadowStateList : ShadowStateAccessor
{
    const rapidjson::Pointer ptr;
    const std::string key;
    const std::function<T *()> itemFactory;
    std::vector<std::unique_ptr<T>> items;

    ShadowStateList(const char *jsonPointer, std::function<T *()> itemFactory)
        : ptr(jsonPointer),
          key(jsonPointer + 1), // strip leading '/'
          itemFactory(std::move(itemFactory))
    {
    }

    ShadowStateList(ShadowStateSet &set, const char *jsonPointer, std::function<T *()> itemFactory)
        : ShadowStateList(jsonPointer, itemFactory)
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
                items.push_back(std::unique_ptr<T>(itemFactory()));
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
    }

    void Store(nvs::NVSHandle &handle) final
    {
    }
};
