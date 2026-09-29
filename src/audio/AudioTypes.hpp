#ifndef AUDIO_TYPES_HPP
#define AUDIO_TYPES_HPP

#include "audio/AudioConstants.hpp"
#include "audio/AudioBuffer.hpp"

namespace anasa
{
    struct AudioBlock
    {
        AudioBlock()
            : AudioBlock(1, MAX_AUDIO_BLOCK_FRAMES)
        {}

        AudioBlock(int channelCount, int frameCapacity)
            : samples(channelCount, frameCapacity)
        {}

        int generation = 0;
        int firstFrame = 0;
        int frameCount = 0;

        AudioBuffer samples;
    };

    struct AudioState
    {
        int generation = 0;
        int expectedBlockStartFrame = 0;
    };
} // namespace anasa

#endif // AUDIO_TYPES_HPP