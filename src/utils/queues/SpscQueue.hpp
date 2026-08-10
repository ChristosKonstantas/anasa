#ifndef SPSC_QUEUE_HPP
#define SPSC_QUEUE_HPP

#include <array>
#include <atomic>

namespace anasa
{

// One producer and one consumer only.
// One slot stays empty so full and empty are easy to distinguish.
template <class T, int capacity>
class SpscQueue
{
    static_assert(capacity >= 2, "SpscQueue capacity must be at least 2");

    public:
        bool push(const T& item)
        {
            int write = _write.load(std::memory_order_relaxed);
            int next = (write + 1) % capacity;

            if (next == _read.load(std::memory_order_acquire))
                return false;

            _data[write] = item;
            _write.store(next, std::memory_order_release);
            return true;
        }

        bool pop(T& item)
        {
            int read = _read.load(std::memory_order_relaxed);

            if (read == _write.load(std::memory_order_acquire))
                return false;

            item = _data[read];
            _read.store((read + 1) % capacity, std::memory_order_release);
            return true;
        }

        bool peek(T& item) const
        {
            /* Returns the front element but leaves it in the queue */
            int read = _read.load(std::memory_order_relaxed);

            if (read == _write.load(std::memory_order_acquire))
                return false;

            item = _data[read];
            return true;
        }

        int usableCapacity() const
        {
            return capacity - 1;
        }

        bool isEmpty() const
        {
            int read = _read.load(std::memory_order_relaxed);
            return read == _write.load(std::memory_order_acquire);
        }

    private:
        std::array<T, capacity> _data{};
        std::atomic<int> _write{0};
        std::atomic<int> _read{0};
};

} // namespace anasa

#endif // SPSC_QUEUE_HPP