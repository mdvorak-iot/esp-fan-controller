#pragma once

template <typename T, int C>
class Average
{
  public:
    const int count = C;

    void add(T value)
    {
        if (_index == -1)
        {
            // First time
            for (_index = 0; _index < C; _index++)
            {
                _values[_index] = value;
            }
            _total = value * C;
            _average = value;
            _index = 0;
        }
        else
        {
            // Advance
            _index = _index == C - 1 ? 0 : _index + 1;
            // Calculate new _total and update
            _total -= _values[_index];
            _total += value;
            _values[_index] = value;
            _average = _total / C;
        }
    }

    inline T value() const
    {
        return _average;
    }

  private:
    T _values[C] = {};
    int _index = -1;
    T _total = 0;
    T _average = 0;
};
