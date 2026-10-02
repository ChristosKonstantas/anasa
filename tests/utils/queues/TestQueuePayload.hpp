#ifndef TEST_QUEUE_PAYLOAD_HPP
#define TEST_QUEUE_PAYLOAD_HPP

#include <cstddef>
#include <vector>

namespace anasa
{
    struct QueueConstructionState
    {
        int failOnAttempt = 0;
        int attempts = 0;
        int alive = 0;
        int destroyed = 0;
        int allocations = 0;
        int deallocations = 0;
    };

    class TestQueuePayload
    {
    public:
        TestQueuePayload(std::size_t size, int value, QueueConstructionState* state = nullptr);
        ~TestQueuePayload();

        TestQueuePayload(const TestQueuePayload&) = delete;
        TestQueuePayload& operator=(const TestQueuePayload&) = delete;
        TestQueuePayload(TestQueuePayload&&) = delete;
        TestQueuePayload& operator=(TestQueuePayload&&) = delete;

        std::vector<int> values;

    private:
        QueueConstructionState* _state;
    };
} // namespace anasa

#endif // TEST_QUEUE_PAYLOAD_HPP