#include "execution/RenderTaskProcessor.hpp"

#include <cassert>

namespace anasa
{
    RenderTaskProcessor::RenderTaskProcessor(const ITileRenderer& renderer)
        : _renderer(renderer)
    {}

    bool RenderTaskProcessor::process(const RenderTask& task, const std::atomic<bool>& stopRequested) const noexcept
    {
        RenderJob& job = *task.job;

        try
        {
            if (!_renderer.renderTile(job, task.tileIndex, stopRequested))
                job.cancel();
        }
        catch (...)
        {
            {
                std::lock_guard<std::mutex> lock(_failureMutex);

                if (!_failure)
                    _failure = std::current_exception();
            }

            job.cancel();
        }

        const int previousTilesRemaining = job.tilesRemaining.fetch_sub(1, std::memory_order_acq_rel);

        assert(previousTilesRemaining > 0);
        return previousTilesRemaining == 1;
    }

    void RenderTaskProcessor::resetFailure()
    {
        std::lock_guard<std::mutex> lock(_failureMutex);
        _failure = nullptr;
    }

    void RenderTaskProcessor::rethrowFailure() const
    {
        std::exception_ptr failure;

        {
            std::lock_guard<std::mutex> lock(_failureMutex);
            failure = _failure;
        }

        if (failure)
            std::rethrow_exception(failure);
    }
}