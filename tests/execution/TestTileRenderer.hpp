#ifndef TEST_TILE_RENDERER_HPP
#define TEST_TILE_RENDERER_HPP    

#include <array>
#include <atomic>

#include "render/ITileRenderer.hpp"
#include "render/RenderConstants.hpp"

namespace anasa
{
    enum class RenderOutcome { Complete, Cancel, Throw };

    class TestTileRenderer final : public ITileRenderer
    {
    public:
        explicit TestTileRenderer(RenderOutcome outcome);

        bool renderTile(RenderJob &job, int tileIndex, const std::atomic<bool> &stopRequested) const override;

        static float sampleForTile(int tileIndex, int channel);

        mutable std::array<std::atomic<int>, TILES_PER_CHUNK> calls{};

    private:
        RenderOutcome _outcome;
    };
    
} // namespace anasa

#endif // TEST_TILE_RENDERER_HPP