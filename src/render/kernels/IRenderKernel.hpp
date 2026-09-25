#ifndef I_RENDER_KERNEL_HPP
#define I_RENDER_KERNEL_HPP

namespace anasa
{
    class IRenderKernel
    {
    public:
        virtual ~IRenderKernel() = default;

        IRenderKernel(const IRenderKernel&) = delete;
        IRenderKernel& operator=(const IRenderKernel&) = delete;
        IRenderKernel(IRenderKernel&&) = delete;
        IRenderKernel& operator=(IRenderKernel&&) = delete;

        // globalFrame is an absolute timeline frame & version identifies the requested content.
        // Calls are synchronous and may run concurrently on the same kernel.
        // Implementations must protect shared mutable state. Failures may throw.
        virtual float renderSample(int globalFrame, int version) const = 0;

    protected:
        IRenderKernel() = default;
    };
} // namespace anasa

#endif // I_RENDER_KERNEL_HPP