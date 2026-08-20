
#ifndef PLAYBACK_TYPES_HPP
#define PLAYBACK_TYPES_HPP

#include <atomic>

namespace anasa
{
    struct SharedState
    {
        std::atomic<bool> stop{false};
        std::atomic<bool> playing{false};

        std::atomic<int> generation{1};
        std::atomic<int> targetFrame{0};
        std::atomic<int> nextUnconsumedFrame{0};
    };

} // namespace anasa

#endif // PLAYBACK_TYPES_HPP