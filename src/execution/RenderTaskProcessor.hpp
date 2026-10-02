#ifndef RENDER_TASK_PROCESSOR_HPP
#define RENDER_TASK_PROCESSOR_HPP

#include <atomic>

#include "execution/ExecutorTypes.hpp"
#include "render/ITileRenderer.hpp"

namespace anasa
{
    class RenderTaskProcessor
    {
    public:
        // The renderer must outlive this processor and every call to process().
        explicit RenderTaskProcessor(const ITileRenderer& renderer);

        // Caller supplies a valid task and processes each tile exactly once.
        // Calls may run concurrently for different tiles.
        // Returns true when this tile finishes the job, including cancelled jobs.
        bool process(const RenderTask& task, const std::atomic<bool>& stopRequested) const noexcept;

    private:
        const ITileRenderer& _renderer;
    };
} // namespace anasa

#endif // RENDER_TASK_PROCESSOR_HPP