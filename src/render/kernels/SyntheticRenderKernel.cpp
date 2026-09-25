#include <cmath>
#include <stdexcept>

#include "render/kernels/SyntheticRenderKernel.hpp"

namespace anasa
{
    constexpr float PI = 3.14159265358979323846f;

    SyntheticRenderKernel::SyntheticRenderKernel(int sampleRate, int workIterations)
        : _sampleRate(sampleRate),
          _workIterations(workIterations)
    {
        if (_sampleRate <= 0)
            throw std::invalid_argument("Sample rate should always be positive");

        if (_workIterations < 0)
            throw std::invalid_argument("Work iterations can not be negative");
    }

    float SyntheticRenderKernel::renderSample(int globalFrame, int version) const
    {
        const float revision = static_cast<float>(version % 13) / 13.0f;
        const float seconds = static_cast<float>(globalFrame) / static_cast<float>(_sampleRate);
        const float baseFrequency = 110.0f + 14.0f * revision;
        const float phase = 2.0f * PI * baseFrequency * seconds;

        float voice = 0.18f * std::sin(phase);
        voice += 0.08f * std::sin(2.0f * phase);
        voice += 0.04f * std::sin(3.0f * phase);

        // Artificial CPU workload used to exercise the scheduler.
        float state = voice + revision;

        for (int iteration = 0; iteration < _workIterations; ++iteration)
        {
            const float drive = 0.001f * static_cast<float>((iteration % 17) - 8);
            state = std::tanh(0.985f * state + drive + 0.01f * revision);
        }

        return std::tanh(voice * (0.8f + 0.2f * state));
    }
} // namespace anasa