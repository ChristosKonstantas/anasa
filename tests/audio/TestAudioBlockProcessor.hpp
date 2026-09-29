#ifndef TEST_AUDIO_BLOCK_PROCESSOR_HPP
#define TEST_AUDIO_BLOCK_PROCESSOR_HPP

#include <atomic>
#include <algorithm>
#include "audio/IAudioBlockProcessor.hpp"

namespace anasa
{
    class TestAudioBlockProcessor final : public IAudioBlockProcessor
    {
    public:
        int blockFrames() const noexcept override { return 128; }

        AudioProcessResult processBlock(AudioOutputBuffer output) noexcept override
        {
            if (output.frameCount == 0)
                return {};

            bool hasOutput = false;

            for (float* channel : output.channels)
            {
                if (channel == nullptr)
                    continue;

                std::fill(channel, channel + output.frameCount, 0.5f);
                hasOutput = true;
            }

            if (!hasOutput)
                return {};

            calls.fetch_add(1, std::memory_order_release);
            return {true, false};
        }

        std::atomic<int> calls{0};
    };
    
} // namespace anasa

#endif //TEST_AUDIO_BLOCK_PROCESSOR_HPP