#ifndef CONTROLLED_RENDER_EXECUTOR_HPP
#define CONTROLLED_RENDER_EXECUTOR_HPP

#include <atomic>
#include <memory>
#include <queue>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "execution/IRenderExecutor.hpp"
#include "execution/RenderTaskProcessor.hpp"

namespace anasa
{
    // Tests decide when accepted tasks finish, without starting worker threads.
    class ControlledRenderExecutor final : public IRenderExecutor
    {
    public:
        explicit ControlledRenderExecutor(const ITileRenderer &renderer);

        bool submit(RenderTask task) override;

        bool popCompleted(std::shared_ptr<RenderJob> &job) override;

        int workerCount() const override;
        int queuedTaskCount() override;

        std::shared_ptr<RenderJob> nextJob() const;

        void completeNextTask();

        bool acceptsTasks = true;

    private:
        RenderTaskProcessor                    _processor;
        std::atomic<bool>                      _stopRequested{false};
        std::queue<RenderTask>                 _tasks;
        std::queue<std::shared_ptr<RenderJob>> _completedJobs;
    };
} // namespace anasa

#endif // CONTROLLED_RENDER_EXECUTOR_HPP