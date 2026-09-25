#include "TestTileRenderer.hpp"

namespace anasa
{
    TestTileRenderer::TestTileRenderer(RenderOutcome outcome) 
        : _outcome(outcome)
    {}

    bool TestTileRenderer::renderTile(RenderJob& job, int tileIndex, const std::atomic<bool>& stopRequested) const
    {
        if (tileIndex < 0 || tileIndex >= TILES_PER_CHUNK || job.chunk != 0)
            throw std::out_of_range("invalid test tile");

        calls[tileIndex].fetch_add(1, std::memory_order_relaxed);

        if (stopRequested.load(std::memory_order_acquire) || job.cancelled.load(std::memory_order_relaxed) || 
                               job.version != 1 || _outcome == RenderOutcome::Cancel)
        {
            job.cancelled.store(true, std::memory_order_relaxed);
            return false;
        }

        if (_outcome == RenderOutcome::Throw)
            throw std::runtime_error("test rendering failure");

        size_t offset = tileIndex * TILE_FRAMES;

        for (size_t i = 0; i < TILE_FRAMES; i++)
            job.samples[offset + i] = sampleForTile(tileIndex);

        return true;
    }

    float TestTileRenderer::sampleForTile(int tileIndex)
    {
        return 0.1f * static_cast<float>(tileIndex + 1);
    }
    
} // namespace anasa