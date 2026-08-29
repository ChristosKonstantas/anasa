#ifndef SPSC_QUEUE_HPP
#define SPSC_QUEUE_HPP

#include <atomic>
#include <memory>
#include <new>
#include <utility>
#include <stdexcept>

namespace anasa
{
/*

 * One producer and one consumer only.
 * _write and _read are monotonically increasing logical cursors.
 * capacity, _read and _write are size_t: they can reach max possible object size in memory 

 * For more information check out this talk
 * https://www.youtube.com/watch?v=K3P_Lmq6pw0&t=764s

*/
template <typename T, typename Alloc = std::allocator<T>>
class SpscQueue : private Alloc // Empty Base Optimization
{        
    static_assert(std::atomic<std::size_t>::is_always_lock_free, "SpscQueue requires lock-free size_t atomics");
    
    public:
        explicit SpscQueue(std::size_t capacity, Alloc const& alloc = Alloc{})
            : Alloc{alloc}, 
              _capacity(validateCapacity(capacity)),
              _powerOfTwo(isPowerOfTwo(_capacity)),
              _mask(_capacity - 1),
              _data{std::allocator_traits<Alloc>::allocate(*this, _capacity)}
        {

            /* Preconstruct all queue slots */
            /* If construction fails, destroy completed objects and release the allocated storage. */

            std::size_t constructed = 0;
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

        /* Gives the producer access to the preconstructed queue-owned object before publishing it. */
        template <typename Fill>
        bool pushWith(Fill&& fill)
        {
            const std::size_t write = _write.load(std::memory_order_relaxed);
            const std::size_t read  = _read.load(std::memory_order_acquire);

            if (isFull(write, read))
                return false;

            std::forward<Fill>(fill)(*element(write));

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

            return element(read);
        }

        const T* front() const
        {
            const std::size_t read = _read.load(std::memory_order_relaxed);

            const std::size_t write = _write.load(std::memory_order_acquire);

            if (isEmpty(write, read))
                return nullptr;

            return element(read);
        }

        /* Consumes the front object without copying it. */
        bool pop()
        {
            const std::size_t read = _read.load(std::memory_order_relaxed);
            const std::size_t write = _write.load(std::memory_order_acquire);

            if (isEmpty(write, read))
                return false;

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

        void reset()
        {
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

        T* element(std::size_t cursor)
        {
            if (_powerOfTwo)
                return &_data[cursor & _mask]; // & is cheaper but capacity should be power of 2

            return &_data[cursor % _capacity];
        }

        const T* element(std::size_t cursor) const
        {
            if (_powerOfTwo)
                return &_data[cursor & _mask]; // & is cheaper but capacity should be power of 2

            return &_data[cursor % _capacity];
        }
        
        static bool isPowerOfTwo(std::size_t value)
        {
            return value != 0 && (value & (value - 1)) == 0;
        }

        static std::size_t validateCapacity(std::size_t capacity)
        {
            if (capacity == 0)
                throw std::invalid_argument("SpscQueue capacity must be greater than zero");

            return capacity;
        }

        std::size_t               _capacity;
        const bool                _powerOfTwo;
        const std::size_t         _mask;
        T*                        _data{}; // heap storage for T objects

        alignas(std::hardware_destructive_interference_size)
        std::atomic<std::size_t>  _write{0};
        alignas(std::hardware_destructive_interference_size)
        std::atomic<std::size_t>  _read{0};
};

} // namespace anasa

#endif // SPSC_QUEUE_HPP