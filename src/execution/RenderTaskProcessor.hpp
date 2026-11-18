#ifndef RENDER_TASK_PROCESSOR_HPP
#define RENDER_TASK_PROCESSOR_HPP

#include <atomic>
#include <exception>
#include <mutex>

#include "execution/ExecutorTypes.hpp"
#include "render/ITileRenderer.hpp"

namespace anasa
{
    class RenderTaskProcessor
    {
    public:
        explicit RenderTaskProcessor(const ITileRenderer& renderer);

        // Caller supplies a valid task and processes each tile exactly once.
        bool process(const RenderTask& task, const std::atomic<bool>& stopRequested) const noexcept;

        // Reset only while workers are stopped.
        void resetFailure();
        void rethrowFailure() const;

    private:
        mutable std::mutex _failureMutex;
        mutable std::exception_ptr _failure;
        const ITileRenderer& _renderer;
    };
}

#endif // RENDER_TASK_PROCESSOR_HPP