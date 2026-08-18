#ifndef SPSC_QUEUE_OLD1_HPP
#define SPSC_QUEUE_OLD1_HPP

#include <atomic>
#include <memory>
#include <cassert>

namespace anasa::old1
{
// One producer and one consumer only.
// _write and _read are monotonically increasing logical cursors.
// capacity, _read and _write are size_t: they can reach max possible object size in memory 
template <typename T, typename Alloc = std::allocator<T>>
class SpscQueue : private Alloc // Empty Base Optimization
{        
    static_assert(std::atomic<std::size_t>::is_always_lock_free, "SpscQueue requires lock-free size_t atomics");
    
    public:
        explicit SpscQueue(std::size_t capacity, Alloc const& alloc = Alloc{})
            : Alloc{alloc}, 
              _capacity(capacity),
              _data{std::allocator_traits<Alloc>::allocate(*this, _capacity)}
        {
            assert(_capacity > 0);

            std::size_t constructed = 0;

            /* Preconstruct all queue slots */
            /* If construction fails, destroy completed objects and release the allocated storage. */
            try
            {
                for (; constructed < _capacity; ++constructed)
                    std::allocator_traits<Alloc>::construct(*this, &_data[constructed]);
            }
            catch (...)
            {
                for (std::size_t i = 0; i < constructed; ++i)
                    std::allocator_traits<Alloc>::destroy(*this, &_data[i]);

                std::allocator_traits<Alloc>::deallocate(*this, _data, _capacity);

                throw;
            }
        }

        ~SpscQueue()
        {
            for (std::size_t i = 0; i < _capacity; ++i)
                std::allocator_traits<Alloc>::destroy(*this, &_data[i]);

            std::allocator_traits<Alloc>::deallocate(*this, _data, _capacity);
        }

        bool push(const T& item)
        {
            const std::size_t write = _write.load(std::memory_order_relaxed);
            const std::size_t read  = _read.load(std::memory_order_acquire);

            if (isFull(write, read))
                return false;

            *element(write) = item; // copy assignment into preconstructed slot

            _write.store(write + 1, std::memory_order_release);
            
            return true;
        }

        bool pop(T& item)
        {
            const std::size_t read =  _read.load(std::memory_order_relaxed);

            const std::size_t write = _write.load(std::memory_order_acquire);

            if (isEmpty(write, read))
                return false;

            item = *element(read); // copy assignment

            _read.store(read + 1, std::memory_order_release);
        
            return true;
        }

        bool peek(T& item) const
        {
            /* Returns the front element but leaves it in the queue */
            std::size_t read = _read.load(std::memory_order_relaxed);

            if (read == _write.load(std::memory_order_acquire))
                return false;

            item = *element(read);

            return true;
        }

        std::size_t capacity() const
        {
            return _capacity;
        }

        bool isEmpty() const
        {
            const std::size_t read  = _read.load(std::memory_order_relaxed);

            const std::size_t write = _write.load(std::memory_order_acquire);

            return isEmpty(write, read);
        }

        bool isFull() const
        {
            const std::size_t write = _write.load(std::memory_order_relaxed);

            const std::size_t read =  _read.load(std::memory_order_acquire);

            return isFull(write, read);
        }

    private:

        bool isFull(std::size_t write, std::size_t read) const
        {
            return write - read == _capacity;
        }

        static bool isEmpty(std::size_t write, std::size_t read)
        {
            return write == read;
        }

        T* element(std::size_t cursor)
        {
            return &_data[cursor % _capacity];
        }

        const T* element(std::size_t cursor) const
        {
            return &_data[cursor % _capacity];
        }

        std::size_t               _capacity;
        T*                        _data{}; // heap storage for T objects

        alignas(std::hardware_destructive_interference_size)
        std::atomic<std::size_t>  _write{0};
        alignas(std::hardware_destructive_interference_size)
        std::atomic<std::size_t>  _read{0};
};

} // namespace anasa::old1

#endif // SPSC_QUEUE_OLD1_HPP