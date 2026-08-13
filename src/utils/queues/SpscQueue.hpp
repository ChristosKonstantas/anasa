#ifndef SPSC_QUEUE_HPP
#define SPSC_QUEUE_HPP

#include <atomic>
#include <memory>
#include <cassert>

namespace anasa
{

// One producer and one consumer only.
// One slot stays empty so full and empty are easy to distinguish.
// capacity, _read and _write are size_t: they can reach max possible object size in memory 
template <typename T, typename Alloc = std::allocator<T>>
class SpscQueue : private Alloc
{
    public:
        explicit SpscQueue(std::size_t capacity, Alloc const& alloc = Alloc{})
            : Alloc{alloc}, 
              _capacity(capacity),
              _data{std::allocator_traits<Alloc>::allocate(*this, _capacity)}
        {
            assert(_capacity > 1);
        }

        ~SpscQueue()
        {
            std::size_t read = _read.load(std::memory_order_relaxed);

            const std::size_t write = _write.load(std::memory_order_relaxed);

            while (read != write)
            {
                std::allocator_traits<Alloc>::destroy(*this, &_data[read]);
                read = (read + 1) % _capacity;
            }

            std::allocator_traits<Alloc>::deallocate(*this, _data, _capacity);
        }

        bool push(const T& item)
        {
            std::size_t write = _write.load(std::memory_order_relaxed);
            std::size_t next = (write + 1) % _capacity;

            if (next == _read.load(std::memory_order_acquire))
                return false;

            std::allocator_traits<Alloc>::construct(*this, &_data[write], item);

            _write.store(next, std::memory_order_release);
            return true;
        }

        bool pop(T& item)
        {
            std::size_t read = _read.load(std::memory_order_relaxed);

            if (read == _write.load(std::memory_order_acquire))
                return false;

            item = _data[read];
            
            std::allocator_traits<Alloc>::destroy(*this, &_data[read]);

            _read.store((read + 1) % _capacity, std::memory_order_release);
        
            return true;
        }

        bool peek(T& item) const
        {
            /* Returns the front element but leaves it in the queue */
            std::size_t read = _read.load(std::memory_order_relaxed);

            if (read == _write.load(std::memory_order_acquire))
                return false;

            item = _data[read]; // copy
            return true;
        }

        std::size_t usableCapacity() const
        {
            return _capacity - 1;
        }

        bool isEmpty() const
        {
            std::size_t read = _read.load(std::memory_order_relaxed);
            return read == _write.load(std::memory_order_acquire);
        }

        bool isFull() const
        {
            const std::size_t write = _write.load(std::memory_order_relaxed);
            const std::size_t next = (write + 1) % _capacity;

            return next == _read.load(std::memory_order_acquire);
        }

    private:
        std::size_t               _capacity;
        T*                        _data{}; // heap storage for T objects
        std::atomic<std::size_t>  _write{0};
        std::atomic<std::size_t>  _read{0};
};

} // namespace anasa

#endif // SPSC_QUEUE_HPP