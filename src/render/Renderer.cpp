#include "render/Renderer.hpp"
#include "render/RenderConstants.hpp"

#include <algorithm>
#include <stdexcept>

namespace anasa
{
    Renderer::Renderer(const IRenderKernel& kernel, const IChunkVersionReader& versionTable)
        : _kernel(kernel),
          _versionTable(versionTable),
          _renderCancellation(_versionTable)
    {}

    bool Renderer::renderTile(RenderJob& job, int tileIndex, const std::atomic<bool>& stopRequested) const
    {
        if (tileIndex < 0 || tileIndex >= TILES_PER_CHUNK)
            throw std::out_of_range("tileIndex is outside the chunk");

        if (job.chunk < 0 || job.chunk >= _versionTable.count())
            throw std::out_of_range("chunk index is outside the timeline");

        if (_renderCancellation.shouldCancel(job, stopRequested))
        {
            job.cancelled.store(true, std::memory_order_relaxed);
            return false;
        }

        const int tileFirstFrame = tileIndex * TILE_FRAMES;
        const int tileLastFrame = std::min(tileFirstFrame + TILE_FRAMES, CHUNK_FRAMES);
        const int chunkFirstFrame = job.chunk * CHUNK_FRAMES;

        for (int frame = tileFirstFrame; frame < tileLastFrame; ++frame)
        {
            const int frameInsideTile = frame - tileFirstFrame;

            if (frameInsideTile % CANCELLATION_CHECK_FRAMES == 0)
            {
                if (_renderCancellation.shouldCancel(job, stopRequested))
                {
                    job.cancelled.store(true, std::memory_order_relaxed);
                    return false;
                }
            }

            const int globalFrame = chunkFirstFrame + frame;
            job.samples[frame] = _kernel.renderSample(globalFrame, job.version);
        }

        // Content may change after the last periodic check.
        if (_renderCancellation.shouldCancel(job, stopRequested))
        {
            job.cancelled.store(true, std::memory_order_relaxed);
            return false;
        }

        return true;
    }
} // namespace anasa