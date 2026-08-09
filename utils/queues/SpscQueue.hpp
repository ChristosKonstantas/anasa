#pragma once

#include <array>
#include <atomic>

namespace lrs
{

// One producer and one consumer only.
// One slot stays empty so full and empty are easy to distinguish.
template <class T, int capacity>
class SpscQueue
{
public:
    bool push(const T &item);

    bool pop(T &item);

    bool peek(T &item) const;

    int usableCapacity() const;

private:
    std::array<T, capacity> _data{};
    std::atomic<int> _write{0};
    std::atomic<int> _read{0};
};

} // namespace lrs