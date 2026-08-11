#ifndef ENGINE_TYPES_HPP
#define ENGINE_TYPES_HPP

#include "engine/EngineSettings.hpp"

#include <array>
#include <atomic>

namespace anasa
{
    struct AudioBlock
    {
        int generation = 0;
        int firstFrame = 0;
        std::array<float, AUDIO_BLOCK_FRAMES> samples{};
    };

    struct AudioState
    {
        int generation = 0;
        int expectedBlockStartFrame = 0;
    };

    struct SharedState
    {
        std::atomic<bool> stop{false};
        std::atomic<bool> playing{false};
        std::atomic<int>  generation{1};
        std::atomic<int>  targetFrame{0};
        std::atomic<int>  nextUnconsumedFrame{0};
    };

    inline int alignToAudioBlock(int frame)
    {
        return frame - frame % AUDIO_BLOCK_FRAMES;
    }

} // namespace anasa

#endif // ENGINE_TYPES_HPP