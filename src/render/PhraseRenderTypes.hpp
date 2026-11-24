#ifndef PHRASE_RENDER_TYPES_HPP
#define PHRASE_RENDER_TYPES_HPP

#include "audio/AudioBuffer.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace anasa
{
    struct PhraseRenderFormat
    {
        int sampleRate = 0;
        int channelCount = 0;
        int hopSize = 0; // Audio frames represented by one conditioning frame.
    };

    struct SingingRequest
    {
        int sampleRate = 0;
        int hopSize = 0;
        std::vector<std::string> phonemes;
        std::vector<std::int64_t> durations; // Conditioning frames per phoneme.
        std::vector<float> f0;              // Hz per conditioning frame; zero means unvoiced.
    };

    struct RenderStageTiming
    {
        std::string name;
        double milliseconds = 0.0;
    };

    struct PhraseRenderResult
    {
        AudioBuffer samples;
        int sampleRate = 0;
        std::vector<RenderStageTiming> stages;
    };

    class PhraseRenderCancelled final : public std::runtime_error
    {
    public:
        PhraseRenderCancelled() : std::runtime_error("Phrase rendering cancelled") {}
    };
}

#endif // PHRASE_RENDER_TYPES_HPP