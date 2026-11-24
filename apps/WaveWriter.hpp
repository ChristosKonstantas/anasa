#ifndef ANASA_APP_WAVE_WRITER_HPP
#define ANASA_APP_WAVE_WRITER_HPP

#include "audio/AudioBuffer.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace anasa::apps
{
    // Offline PCM16 export; never call from an audio callback.
    inline void writeStereoWave(const std::filesystem::path& path, const AudioBuffer& samples, int sampleRate)
    {
        static_assert(std::endian::native == std::endian::little);
        if (samples.channelCount() != 2 || samples.frameCount() <= 0 || sampleRate <= 0)
            throw std::invalid_argument("Expected non-empty stereo PCM and a positive sample rate");
        const auto byteCount = static_cast<std::uint64_t>(samples.frameCount()) * 4;
        if (byteCount > std::numeric_limits<std::uint32_t>::max() - 36 ||
            static_cast<std::uint64_t>(sampleRate) * 4 > std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("Audio exceeds PCM WAV limits");

        for (int channel = 0; channel < 2; ++channel)
            for (float sample : samples[channel])
                if (!std::isfinite(sample))
                    throw std::invalid_argument("Cannot export non-finite audio");

        std::ofstream file(path, std::ios::binary);
        if (!file)
            throw std::runtime_error("Cannot create WAV: " + path.string());

        const auto write = [&file](auto value)
        {
            file.write(reinterpret_cast<const char*>(&value), sizeof(value));
        };

        const auto bytes = static_cast<std::uint32_t>(byteCount);
        file.write("RIFF", 4);
        write(std::uint32_t{36} + bytes);
        file.write("WAVEfmt ", 8);
        write(std::uint32_t{16});
        write(std::uint16_t{1});
        write(std::uint16_t{2});
        write(static_cast<std::uint32_t>(sampleRate));
        write(static_cast<std::uint32_t>(sampleRate) * 4);
        write(std::uint16_t{4});
        write(std::uint16_t{16});

        file.write("data", 4);
        write(bytes);
        for (int frame = 0; frame < samples.frameCount(); ++frame)
            for (int channel = 0; channel < 2; ++channel)
                write(static_cast<std::int16_t>(std::lround(
                    std::clamp(samples[channel][frame], -1.0f, 1.0f) * 32767.0f)));
        file.close();
        
        if (!file)
            throw std::runtime_error("Failed to finish WAV: " + path.string());
    }
}

#endif // ANASA_APP_WAVE_WRITER_HPP