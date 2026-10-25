#include "ControlledRenderExecutor.hpp"

namespace anasa
{
    ControlledRenderExecutor::ControlledRenderExecutor(const ITileRenderer& renderer)
        : _processor(renderer)
    {}

    bool ControlledRenderExecutor::submit(RenderTask task)
    {
        if (!acceptsTasks)
            return false;
        _tasks.push(std::move(task));
        return true;
    }

    bool ControlledRenderExecutor::popCompleted(std::shared_ptr<RenderJob>& job)
    {
        if (_completedJobs.empty())
            return false;
        job = std::move(_completedJobs.front());
        _completedJobs.pop();
        return true;
    }

    int ControlledRenderExecutor::workerCount() const { return 1; }
    int ControlledRenderExecutor::queuedTaskCount() { return static_cast<int>(_tasks.size()); }

    std::shared_ptr<RenderJob> ControlledRenderExecutor::nextJob() const
    {
        REQUIRE_FALSE(_tasks.empty());
        return _tasks.front().job;
    }

    void ControlledRenderExecutor::completeNextTask()
    {
        REQUIRE_FALSE(_tasks.empty());
        RenderTask task = std::move(_tasks.front());
        _tasks.pop();
        if (_processor.process(task, _stopRequested))
            _completedJobs.push(std::move(task.job));
    }
} // namespace anasa