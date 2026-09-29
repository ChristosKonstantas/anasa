#ifndef AUDIO_BUFFER_HPP
#define AUDIO_BUFFER_HPP

#include <cassert>
#include <span>
#include <stdexcept>
#include <vector>

namespace anasa
{
    // Owns planar samples: each channel occupies frameCount contiguous floats.
    // Construction and assignment may allocate. Keep them outside the audio callback.
    class AudioBuffer
    {
    public:
        AudioBuffer() = default;

        AudioBuffer(int channelCount, int frameCount);

        int channelCount() const noexcept;

        int frameCount() const noexcept
        {
            return _samples.empty() ? 0 : _frameCount;
        }

        // Borrowed views. The index must be in [0, channelCount()).
        // Do not retain views across owner assignment, movement or destruction.
        std::span<float> channel(int index) noexcept;

        std::span<const float> channel(int index) const noexcept;

        std::span<float> operator[](int channel) noexcept
        {
            return this->channel(channel);
        }

        std::span<const float> operator[](int channel) const noexcept
        {
            return this->channel(channel);
        }

    private:
        static std::size_t sampleCount(int channelCount, int frameCount);

        std::vector<float> _samples;
        int                _frameCount = 0;
    };
} // namespace anasa

#endif // AUDIO_BUFFER_HPP
