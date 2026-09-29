#ifndef AUDIO_OUTPUT_BUFFER_HPP
#define AUDIO_OUTPUT_BUFFER_HPP

#include <span>

namespace anasa
{
    // Borrowed planar output: one pointer per channel, frameCount samples per pointer.
    // Null channel pointers represent disabled outputs. Valid only during processing.
    struct AudioOutputBuffer
    {
        std::span<float* const> channels;
        int frameCount = 0;
    };
}

#endif // AUDIO_OUTPUT_BUFFER_HPP