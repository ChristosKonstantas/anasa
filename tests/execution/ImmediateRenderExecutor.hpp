#ifndef IMMEDIATE_RENDER_EXECUTOR_HPP
#define IMMEDIATE_RENDER_EXECUTOR_HPP

#include <atomic>
#include <memory>
#include <queue>
#include <utility>

#include "execution/IRenderExecutor.hpp"
#include "execution/RenderTaskProcessor.hpp"

namespace anasa
{
    // Complete accepted work inline so scheduling tests do not depend on timing.
    class ImmediateRenderExecutor final : public IRenderExecutor
    {
    public:
        explicit ImmediateRenderExecutor(const ITileRenderer &renderer);

        bool submit(RenderTask task) override;

        bool popCompleted(std::shared_ptr<RenderJob> &job) override;

        int workerCount() const override { return 1; }
        int queuedTaskCount() override { return 0; }

    private:
        RenderTaskProcessor                    _processor;
        std::atomic<bool>                      _stopRequested{false};
        std::queue<std::shared_ptr<RenderJob>> _completedJobs;
    };
    
} // namespace anasa

#endif // IMMEDIATE_RENDER_EXECUTOR_HPP