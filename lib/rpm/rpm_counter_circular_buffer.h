#pragma once

#include <stddef.h>

namespace rpmcounter
{

// This class is not thread-safe
template <typename T, size_t C>
class rpm_counter_circular_buffer
{
public:
    rpm_counter_circular_buffer()
        : index_(0), values_{0}
    {
    }

    inline size_t size() const
    {
        return C;
    }

    void push(const T &value)
    {
        index_ = (index_ + 1) % C;
        values_[index_] = value;
    }

    T first() const
    {
        return values_[(index_ + 1) % C];
    }

    const T *values() const
    {
        return values_;
    }

private:
    size_t index_;
    T values_[C];
};

}; // namespace rpmcounter
