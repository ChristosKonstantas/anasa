#ifndef SPSC_QUEUE_HPP
#define SPSC_QUEUE_HPP

#include <atomic>
#include <memory>
#include <new>
#include <utility>
#include <stdexcept>
#include <concepts>

namespace anasa
{
/*

 * One producer and one consumer only.
 * _write and _read are unsigned logical counters that may wrap.
 * Producer and consumer own separate physical slot positions.
 * reset() and destruction require both threads to be stopped.

 * For more information check out this talk
 * https://www.youtube.com/watch?v=K3P_Lmq6pw0&t=764s

*/
template <typename T, typename Alloc = std::allocator<T>>
class SpscQueue : private Alloc // Empty Base Optimization
{        
    static_assert(std::atomic<std::size_t>::is_always_lock_free, "SpscQueue requires lock-free size_t atomics");
    
    public:
        // Construct every slot from the same arguments before concurrent use.
        template <typename... Args>
        requires std::constructible_from<T, const Args&...>
        explicit SpscQueue(std::size_t capacity, const Args&... args)
            : SpscQueue(std::allocator_arg, Alloc{}, capacity, args...)
        {}

        template <typename... Args>
        SpscQueue(std::allocator_arg_t, Alloc const& alloc, std::size_t capacity, const Args&... args)
            : Alloc{alloc},
              _capacity(validateCapacity(capacity)),
              _data{std::allocator_traits<Alloc>::allocate(*this, _capacity)}
        {
            /* Preconstruct all queue slots */
            /* If construction fails, destroy completed objects and release the allocated storage. */

            std::size_t constructed = 0;

            try
            {
                for (; constructed < _capacity; ++constructed)
                    std::allocator_traits<Alloc>::construct(*this, &_data[constructed], args...);
            }
            catch (...)
            {
                for (std::size_t i = 0; i < constructed; ++i)
                    std::allocator_traits<Alloc>::destroy(*this, &_data[i]);

                std::allocator_traits<Alloc>::deallocate(*this, _data, _capacity);

                throw;
            }
        }

        SpscQueue(const SpscQueue&) = delete;

        SpscQueue& operator=(const SpscQueue&) = delete;
        
        SpscQueue(SpscQueue&&) = delete;
        
        SpscQueue& operator=(SpscQueue&&) = delete;
        
        ~SpscQueue()
        {
            for (std::size_t i = 0; i < _capacity; ++i)
                std::allocator_traits<Alloc>::destroy(*this, &_data[i]);

            std::allocator_traits<Alloc>::deallocate(*this, _data, _capacity);
        }

        /* Gives the producer access to the preconstructed queue-owned object before publishing it. */
        template <typename Fill>
        bool pushWith(Fill&& fill)
        {
            const std::size_t write = _write.load(std::memory_order_relaxed);
            const std::size_t read  = _read.load(std::memory_order_acquire);

            if (isFull(write, read))
                return false;

            std::forward<Fill>(fill)(_data[_writeSlot]);
            _writeSlot = nextSlot(_writeSlot);

            _write.store(write + 1, std::memory_order_release);

            return true;
        }

        /* Returns the queue-owned front object. Valid until pop() is called by the consumer. */
        T* front()
        {
            const std::size_t read = _read.load(std::memory_order_relaxed);

            const std::size_t write = _write.load(std::memory_order_acquire);

            if (isEmpty(write, read))
                return nullptr;

            return &_data[_readSlot];
        }

        const T* front() const
        {
            const std::size_t read = _read.load(std::memory_order_relaxed);

            const std::size_t write = _write.load(std::memory_order_acquire);

            if (isEmpty(write, read))
                return nullptr;

            return &_data[_readSlot];
        }

        /* Consumes the front object without copying it. */
        bool pop()
        {
            const std::size_t read = _read.load(std::memory_order_relaxed);
            const std::size_t write = _write.load(std::memory_order_acquire);

            if (isEmpty(write, read))
                return false;

            _readSlot = nextSlot(_readSlot);
            _read.store(read + 1, std::memory_order_release);

            return true;
        }

        std::size_t capacity() const
        {
            return _capacity;
        }

        bool isEmpty() const
        {
            const std::size_t read =  _read.load(std::memory_order_relaxed);

            const std::size_t write = _write.load(std::memory_order_acquire);

            return isEmpty(write, read);
        }

        bool isFull() const
        {
            const std::size_t write = _write.load(std::memory_order_relaxed);

            const std::size_t read =  _read.load(std::memory_order_acquire);

            return isFull(write, read);
        }

        // No producer or consumer may access the queue during reset.
        void reset()
        {
            _readSlot = 0;
            _writeSlot = 0;
            _read.store(0, std::memory_order_relaxed);
            _write.store(0, std::memory_order_relaxed);
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

        std::size_t nextSlot(std::size_t slot) const noexcept
        {
            return slot + 1 == _capacity ? 0 : slot + 1;
        }

        static std::size_t validateCapacity(std::size_t capacity)
        {
            if (capacity == 0)
                throw std::invalid_argument("SpscQueue capacity must be greater than zero");

            return capacity;
        }

    std::size_t               _capacity;
    T*                        _data{}; // heap storage for T objects

    alignas(std::hardware_destructive_interference_size)
    std::atomic<std::size_t>  _write{0};
    std::size_t               _writeSlot{0}; // producer only
    alignas(std::hardware_destructive_interference_size)
    std::atomic<std::size_t>  _read{0};
    std::size_t               _readSlot{0}; // consumer only
};

} // namespace anasa

#endif // SPSC_QUEUE_HPP