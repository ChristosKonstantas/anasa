#ifndef I_RENDER_EXECUTOR_HPP
#define I_RENDER_EXECUTOR_HPP

#include <memory>

#include "execution/ExecutorTypes.hpp"

namespace anasa
{
    // Scheduler-facing operations. The owner controls startup and shutdown.
    class IRenderExecutor
    {
    public:
        virtual ~IRenderExecutor() = default;

        IRenderExecutor(const IRenderExecutor&) = delete;
        IRenderExecutor& operator=(const IRenderExecutor&) = delete;
        IRenderExecutor(IRenderExecutor&&) = delete;
        IRenderExecutor& operator=(IRenderExecutor&&) = delete;

        // Returns false when work cannot be accepted & invalid tasks throw.
        // The caller must not resubmit an accepted tile of the same job.
        virtual bool submit(RenderTask task) = 0;

        // Publishes each job once, after all tiles finish, including failed/cancelled tiles.
        // Sample writes are visible on success. False leaves job unchanged.
        // Shutdown may discard queued tasks without publishing their jobs.
        virtual bool popCompleted(std::shared_ptr<RenderJob>& job) = 0;

        // Fixed, positive worker count used by Scheduler to limit dispatch.
        virtual int workerCount() const = 0;

        // Snapshot of waiting tasks, excluding tasks already running.
        virtual int queuedTaskCount() = 0;

    protected:
        IRenderExecutor() = default;
    };
} // namespace anasa

#endif // I_RENDER_EXECUTOR_HPP