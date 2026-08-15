#ifndef SPSC_QUEUE_HPP
#define SPSC_QUEUE_HPP

#include <atomic>
#include <memory>
#include <cassert>

namespace anasa
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
        }

        ~SpscQueue()
        {
            std::size_t read = _read.load(std::memory_order_relaxed);

            const std::size_t write = _write.load(std::memory_order_relaxed);

            while (read != write) // notEmpty
            {
                std::allocator_traits<Alloc>::destroy(*this, element(read));
                ++read;
            }

            std::allocator_traits<Alloc>::deallocate(*this, _data, _capacity);
        }

        bool push(const T& item)
        {
            const std::size_t write = _write.load(std::memory_order_relaxed);
            const std::size_t read  = _read.load(std::memory_order_acquire);

            if (isFull(write, read))
                return false;

            std::allocator_traits<Alloc>::construct(*this, element(write), item); // new (&_data[_write % _capacity]) T(item); - copy construct the value

            _write.store(write + 1, std::memory_order_release);
            
            return true;
        }

        bool pop(T& item)
        {
            const std::size_t read =  _read.load(std::memory_order_relaxed);

            const std::size_t write = _write.load(std::memory_order_acquire);

            if (isEmpty(write, read))
                return false;

            T* current = element(read);

            item = *current; // copy assignment
            
            std::allocator_traits<Alloc>::destroy(*this, current); // destroy the instance that was in &_data[read]

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

} // namespace anasa

#endif // SPSC_QUEUE_HPP