#include <cstddef>
#include <stdexcept>

#include "render/RenderTypes.hpp"
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

        // The caller must turn a false result into job cancellation.
        if (_outcome == RenderOutcome::Cancel)
            return false;

        if (stopRequested.load(std::memory_order_acquire) || job.cancelled.load(std::memory_order_relaxed) || job.version != 1)
        {
            job.cancelled.store(true, std::memory_order_relaxed);
            return false;
        }

        if (_outcome == RenderOutcome::Throw)
            throw std::runtime_error("test rendering failure");

        std::size_t offset = tileIndex * TILE_FRAMES;

        for (int channel = 0; channel < job.samples.channelCount(); ++channel)
        {
            for (std::size_t i = 0; i < TILE_FRAMES; i++)
                job.samples[channel][offset + i] = sampleForTile(tileIndex, channel);
        }

        return true;
    }

    float TestTileRenderer::sampleForTile(int tileIndex, int channel)
    {
        return 0.1f * static_cast<float>(tileIndex + 1) + 0.01f * static_cast<float>(channel);
    }
    
} // namespace anasa