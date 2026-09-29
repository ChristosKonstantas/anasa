#include "TestRenderKernel.hpp"

namespace anasa
{
    float TestRenderKernel::renderSample(int channel, int globalFrame, int version) const
    {
        _callCount.fetch_add(1, std::memory_order_relaxed);

        return static_cast<float>(globalFrame) / 16384.0f + static_cast<float>(version) / 128.0f + static_cast<float>(channel) / 32.0f;
    }

    int TestRenderKernel::callCount() const
    {
        return _callCount.load(std::memory_order_relaxed);
    }
} // namespace anasa