#include "ImmediateRenderExecutor.hpp"

namespace anasa
{
    ImmediateRenderExecutor::ImmediateRenderExecutor(const ITileRenderer& renderer)
        : _processor(renderer)
    {}

    bool ImmediateRenderExecutor::submit(RenderTask task) 
    {
        if (_processor.process(task, _stopRequested))
            _completedJobs.push(std::move(task.job));
        return true;
    }

    bool ImmediateRenderExecutor::popCompleted(std::shared_ptr<RenderJob>& job)
    {
        if (_completedJobs.empty())
            return false;

        job = std::move(_completedJobs.front());
        _completedJobs.pop();
        
        return true;
    }

} // namespace anasa