#ifndef I_AUDIO_BLOCK_PROCESSOR_HPP
#define I_AUDIO_BLOCK_PROCESSOR_HPP

#include "audio/AudioOutputBuffer.hpp"

namespace anasa
{
    struct AudioProcessResult
    {
        bool playing = false;
        bool underrun = false;
    };

    // Processes one internal block across the enabled output channels.
    // Device callback sizes are a separate concern.
    class IAudioBlockProcessor
    {
    public:
        virtual ~IAudioBlockProcessor() = default;

        IAudioBlockProcessor(const IAudioBlockProcessor&) = delete;
        IAudioBlockProcessor& operator=(const IAudioBlockProcessor&) = delete;
        IAudioBlockProcessor(IAudioBlockProcessor&&) = delete;
        IAudioBlockProcessor& operator=(IAudioBlockProcessor&&) = delete;

        // Fixed, positive size for this processor's lifetime.
        virtual int blockFrames() const noexcept = 0;

        // One serialized consumer. frameCount must be zero or equal blockFrames().
        // Each non-null channel points to at least frameCount writable floats.
        // Writes every enabled output sample, including silence. Retains no pointers.
        // Zero frames or no enabled channels: no consumption, no timeline advance.
        // No allocation, blocking, I/O, or exceptions on this path.
        // playing means the timeline advanced, underrun implies playing.
        virtual AudioProcessResult processBlock(AudioOutputBuffer output) noexcept = 0;

    protected:
        IAudioBlockProcessor() = default;
    };
}

#endif // I_AUDIO_BLOCK_PROCESSOR_HPP