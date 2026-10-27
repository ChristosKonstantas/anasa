#ifndef I_TILE_RENDERER_HPP
#define I_TILE_RENDERER_HPP

#include <atomic>

namespace anasa
{
    struct RenderJob;

    // Synchronous tile output contract. Executor workers may call the same renderer concurrently.
    class ITileRenderer
    {
    public:
        virtual ~ITileRenderer() = default;

        ITileRenderer(const ITileRenderer&) = delete;
        ITileRenderer& operator=(const ITileRenderer&) = delete;
        ITileRenderer(ITileRenderer&&) = delete;
        ITileRenderer& operator=(ITileRenderer&&) = delete;

        // Write only the requested tile, across all channels. Storage dimensions remain fixed.
        // Return true only when the complete tile is usable. False cancels the job; it is not a "pending" result. 
        // Exceptions also cause Executor to cancel the job. Implementations observe stopRequested, job cancellation and content invalidation.
        virtual bool renderTile(RenderJob& job, int tileIndex, const std::atomic<bool>& stopRequested) const = 0;

    protected:
        ITileRenderer() = default;
    };
}

#endif // I_TILE_RENDERER_HPP