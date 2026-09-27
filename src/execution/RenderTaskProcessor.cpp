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
                job.cancelled.store(true, std::memory_order_release);
        }
        catch (...)
        {
            // Renderer failure will cancel the job, but this tile still finishes.
            job.cancelled.store(true, std::memory_order_release);
        }

        const int previousTilesRemaining = job.tilesRemaining.fetch_sub(1, std::memory_order_acq_rel);
        assert(previousTilesRemaining > 0);

        return previousTilesRemaining == 1;
    }
} // namespace anasa