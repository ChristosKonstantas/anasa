#include <stdexcept>

#include "TestQueuePayload.hpp"

namespace anasa
{
    TestQueuePayload::TestQueuePayload(std::size_t size, int value, QueueConstructionState* state)
        : values(size, value),
          _state(state)
    {
        if (_state == nullptr)
            return;

        if (++_state->attempts == _state->failOnAttempt)
            throw std::runtime_error("slot construction failed");

        ++_state->alive;
    }

    TestQueuePayload::~TestQueuePayload()
    {
        if (_state == nullptr)
            return;

        --_state->alive;
        ++_state->destroyed;
    }
} // namespace anasa