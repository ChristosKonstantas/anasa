#ifndef OPEN_VPI_RENDERER_HPP
#define OPEN_VPI_RENDERER_HPP

#include "audio/AudioBuffer.hpp"
#include <onnxruntime_cxx_api.h>
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace anasa
{
    struct SingingRequest
    {
        std::vector<std::string> phonemes;
        std::vector<std::int64_t> durations; // Mel frames per phoneme.
        std::vector<float> f0;              // Hz per mel frame; zero for silence.
    };

    struct OpenVpiRenderResult
    {
        AudioBuffer samples; // Planar stereo, identical left/right channels.
        double acousticMs = 0.0;
        double vocoderMs = 0.0;
    };

    // TIGER v106/fresh CPU adapter. One caller at a time; never call on the audio thread.
    // Renders whole phrases. This is not an ITileRenderer implementation.
    class OpenVpiRenderer final
    {
    public:
        static constexpr int SAMPLE_RATE = 44100;
        static constexpr int HOP_SIZE = 512;
        static constexpr int CHANNELS = 2;
        static constexpr int MAX_MEL_FRAMES = 4096;

        OpenVpiRenderer(const std::filesystem::path& voicebank, int threads, int steps);
        OpenVpiRenderResult render(const SingingRequest& request);

    private:
        std::int64_t _steps;
        Ort::Env _environment;
        Ort::SessionOptions _options;
        Ort::Session _acoustic;
        Ort::Session _vocoder;
        std::array<float, 256> _speaker;
        std::unordered_map<std::string, std::int64_t> _phonemes;
    };
}

#endif // OPEN_VPI_RENDERER_HPP