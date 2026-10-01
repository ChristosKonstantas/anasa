#ifndef TEST_QUEUE_ALLOCATOR_HPP
#define TEST_QUEUE_ALLOCATOR_HPP

#include <memory>

#include "TestQueuePayload.hpp"

namespace anasa
{
    template <typename T>
    class TestQueueAllocator
    {
    public:
        using value_type = T;

        explicit TestQueueAllocator(QueueConstructionState* state) : 
            _state(state) 
        {}

        template <typename U>
        TestQueueAllocator(const TestQueueAllocator<U>& other) noexcept : 
            _state(other._state) 
        {}

        T* allocate(std::size_t count)
        {
            T* result = std::allocator<T>{}.allocate(count);
            ++_state->allocations;
            return result;
        }

        void deallocate(T* data, std::size_t count) noexcept
        {
            ++_state->deallocations;
            std::allocator<T>{}.deallocate(data, count);
        }

        template <typename U>
        bool operator==(const TestQueueAllocator<U>& other) const noexcept
        {
            return _state == other._state;
        }

    private:
        template <typename U>
        friend class TestQueueAllocator;

        QueueConstructionState* _state;
    };
} // namespace anasa

#endif // TEST_QUEUE_ALLOCATOR_HPP