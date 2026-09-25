#ifndef TEST_RENDER_KERNEL_HPP
#define TEST_RENDER_KERNEL_HPP

#include <atomic>

#include "render/kernels/IRenderKernel.hpp"

namespace anasa
{
    class TestRenderKernel final : public IRenderKernel
    {
    public:
        float renderSample(int globalFrame, int version) const override;
        int   callCount() const;

    private:
        mutable std::atomic<int> _callCount{0};
    };
} // namespace anasa

#endif // TEST_RENDER_KERNEL_HPP