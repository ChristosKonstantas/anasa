#ifndef SYNTHETIC_RENDER_KERNEL_HPP
#define SYNTHETIC_RENDER_KERNEL_HPP

#include "render/kernels/IRenderKernel.hpp"

namespace anasa
{
    class SyntheticRenderKernel final : public IRenderKernel
    {
    public:
        SyntheticRenderKernel(int sampleRate, int workIterations);

        float renderSample(int globalFrame, int version) const override;

    private:
        int _sampleRate;
        int _workIterations;
    };
} // namespace anasa

#endif // SYNTHETIC_RENDER_KERNEL_HPP