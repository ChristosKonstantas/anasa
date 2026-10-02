#include "audio/AudioBuffer.hpp"

namespace anasa
{
    AudioBuffer::AudioBuffer(int channelCount, int frameCount)
        : _samples(sampleCount(channelCount, frameCount)),
          _frameCount(frameCount)
    {
    }

    int AudioBuffer::channelCount() const noexcept
    {
        return _samples.empty() ? 0 : static_cast<int>(_samples.size() / static_cast<std::size_t>(_frameCount));
    }

    std::span<float> AudioBuffer::channel(int index) noexcept
    {
        assert(index >= 0 && index < channelCount());
        return std::span<float>(_samples).subspan(static_cast<std::size_t>(index) * _frameCount, _frameCount);
    }

    std::span<const float> AudioBuffer::channel(int index) const noexcept
    {
        assert(index >= 0 && index < channelCount());
        return std::span<const float>(_samples).subspan(static_cast<std::size_t>(index) * _frameCount, _frameCount);
    }

    std::size_t AudioBuffer::sampleCount(int channelCount, int frameCount)
    {
        if (channelCount <= 0 || frameCount <= 0)
            throw std::invalid_argument("Audio buffer dimensions must be greater than zero");

        const std::size_t channels = static_cast<std::size_t>(channelCount);
        const std::size_t frames = static_cast<std::size_t>(frameCount);

        if (channels > std::vector<float>().max_size() / frames)
            throw std::length_error("Audio buffer dimensions exceed the supported size");

        return channels * frames;
    }


}// namespace anasa

