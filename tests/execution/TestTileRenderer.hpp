#ifndef TEST_TILE_RENDERER_HPP
#define TEST_TILE_RENDERER_HPP    

#include "execution/Executor.hpp"
#include "render/ITileRenderer.hpp"

namespace anasa
{
    enum class RenderOutcome { Complete, Cancel, Throw };

    // This executable does not include or link the concrete Renderer.
    class TestTileRenderer final : public ITileRenderer
    {
    public:
        explicit TestTileRenderer(RenderOutcome outcome);

        bool renderTile(RenderJob &job, int tileIndex, const std::atomic<bool> &stopRequested) const override;

        static float sampleForTile(int tileIndex);

        mutable std::array<std::atomic<int>, TILES_PER_CHUNK> calls{};

    private:
        RenderOutcome _outcome;
    };
    
} // namespace anasa

#endif // TEST_TILE_RENDERER_HPP