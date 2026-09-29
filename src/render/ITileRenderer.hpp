#ifndef I_TILE_RENDERER_HPP
#define I_TILE_RENDERER_HPP

#include <atomic>

namespace anasa
{
    struct RenderJob;

    // Synchronous rendering contract shared by synthetic ML-inference renderers.
    class ITileRenderer
    {
    public:
        virtual ~ITileRenderer() = default;

        ITileRenderer(const ITileRenderer&) = delete;
        ITileRenderer& operator=(const ITileRenderer&) = delete;
        ITileRenderer(ITileRenderer&&) = delete;
        ITileRenderer& operator=(ITileRenderer&&) = delete;

        virtual bool renderTile(RenderJob& job, int tileIndex, const std::atomic<bool>& stopRequested) const = 0;

    protected:
        ITileRenderer() = default;
    };
}

#endif // I_TILE_RENDERER_HPP