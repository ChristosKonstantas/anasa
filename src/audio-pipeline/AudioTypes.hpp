#ifndef AUDIO_TYPES_HPP
#define AUDIO_TYPES_HPP

#include <array>
#include <atomic>

#include "audio-pipeline/AudioConstants.hpp"

namespace anasa
{
    struct AudioBlock
    {
        int generation = 0;
        int firstFrame = 0;
        int frameCount = 0;

        std::array<float, MAX_AUDIO_BLOCK_FRAMES> samples{};
    };

    struct AudioState
    {
        int generation = 0;
        int expectedBlockStartFrame = 0;
    };


} // namespace anasa

#endif // AUDIO_TYPES_HPP