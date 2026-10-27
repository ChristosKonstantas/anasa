#ifndef PCM_RENDER_KERNEL_HPP
#define PCM_RENDER_KERNEL_HPP

#include "audio/AudioBuffer.hpp"
#include "render/kernels/IRenderKernel.hpp"
#include <stdexcept>
#include <utility>

namespace anasa
{
    // Immutable PCM for one content revision, starting at timeline frame zero.
    // Caller must use the PCM's sample rate. This class does not resample or infer.
    class PcmRenderKernel final : public IRenderKernel
    {
    public:
        PcmRenderKernel(AudioBuffer samples, int version)
            : _samples(std::move(samples)), _version(version)
        {
            if (_samples.channelCount() <= 0 || _samples.frameCount() <= 0)
                throw std::invalid_argument("PCM must not be empty");
        }

        float renderSample(int channel, int globalFrame, int version) const override
        {
            if (version != _version)
                throw std::runtime_error("PCM is unavailable for this content version");
            if (channel < 0 || channel >= _samples.channelCount() || globalFrame < 0)
                throw std::out_of_range("Invalid PCM sample position");
            return globalFrame < _samples.frameCount() ? _samples[channel][globalFrame] : 0.0f;
        }

    private:
        const AudioBuffer _samples;
        const int _version;
    };
}

#endif // PCM_RENDER_KERNEL_HPP