#ifndef SPSC_QUEUE_HPP
#define SPSC_QUEUE_HPP

#include <array>
#include <atomic>

namespace anasa
{

// One producer and one consumer only.
// One slot stays empty so full and empty are easy to distinguish.
// capacity, _read and _write are size_t: they can reach max possible object size in memory 
template <class T, size_t capacity>
class SpscQueue
{
    static_assert(capacity >= 2, "SpscQueue capacity must be at least 2");

    public:
        bool push(const T& item)
        {
            size_t write = _write.load(std::memory_order_relaxed);
            size_t next = (write + 1) % capacity;

            if (next == _read.load(std::memory_order_acquire))
                return false;

            _data[write] = item;
            _write.store(next, std::memory_order_release);
            return true;
        }

        bool pop(T& item)
        {
            size_t read = _read.load(std::memory_order_relaxed);

            if (read == _write.load(std::memory_order_acquire))
                return false;

            item = _data[read]; // copy
            _read.store((read + 1) % capacity, std::memory_order_release);
            return true;
        }

        bool peek(T& item) const
        {
            /* Returns the front element but leaves it in the queue */
            size_t read = _read.load(std::memory_order_relaxed);

            if (read == _write.load(std::memory_order_acquire))
                return false;

            item = _data[read]; // copy
            return true;
        }

        size_t usableCapacity() const
        {
            return capacity - 1;
        }

        bool isEmpty() const
        {
            size_t read = _read.load(std::memory_order_relaxed);
            return read == _write.load(std::memory_order_acquire);
        }

        bool isFull() const
        {
            const size_t write = _write.load(std::memory_order_relaxed);
            const size_t next = (write + 1) % capacity;

            return next == _read.load(std::memory_order_acquire);
        }
        
    private:
        std::array<T, capacity> _data{};
        std::atomic<size_t> _write{0};
        std::atomic<size_t> _read{0};
};

} // namespace anasa

#endif // SPSC_QUEUE_HPP