#ifndef OPEN_VPI_RENDERER_HPP
#define OPEN_VPI_RENDERER_HPP

#include "render/IPhraseRenderer.hpp"
#include <onnxruntime_cxx_api.h>
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace anasa
{
    // TIGER v106/fresh CPU adapter. One caller at a time; never call on the audio thread.
    // Renders whole phrases. This is not an ITileRenderer implementation.
    class OpenVpiRenderer final : public IPhraseRenderer
    {
    public:
        static constexpr int SAMPLE_RATE = 44100;
        static constexpr int HOP_SIZE = 512;
        static constexpr int CHANNELS = 2;
        static constexpr int MAX_MEL_FRAMES = 4096;

        OpenVpiRenderer(const std::filesystem::path& voicebank, int threads, int steps);

        PhraseRenderFormat format() const noexcept override;
        PhraseRenderResult render(const SingingRequest& request, std::stop_token stopToken = {}) override;

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