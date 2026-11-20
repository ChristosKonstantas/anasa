#ifndef PHRASE_TILE_RENDERER_HPP
#define PHRASE_TILE_RENDERER_HPP

#include "render/IPhraseRenderer.hpp"
#include "render/ITileRenderer.hpp"
#include "render/PreparedSingingTimeline.hpp"
#include "render/RenderCancellation.hpp"
#include <chrono>
#include <memory>
#include <mutex>

namespace anasa
{
    struct PhraseRenderMetrics
    {
        long long inferenceCalls = 0;
        long long completedPhrases = 0;
        long long cancelledPhrases = 0;
        long long failedPhrases = 0;
        double renderMilliseconds = 0.0;
    };

    // One entire phrase per job, with one tile per chunk. Scheduler owns the PCM cache.
    class PhraseTileRenderer final : public ITileRenderer
    {
    public:
        PhraseTileRenderer(std::unique_ptr<IPhraseRenderer> renderer, std::shared_ptr<const PreparedSingingTimeline> timeline, const IChunkVersionReader& versions);

        bool renderTile(RenderJob& job, int tileIndex, const std::atomic<bool>& stopRequested) const override;

        PhraseRenderMetrics metrics() const;

    private:
        using Clock = std::chrono::steady_clock;

        void recordElapsed(Clock::time_point start) const;

        std::unique_ptr<IPhraseRenderer>               _renderer;
        std::shared_ptr<const PreparedSingingTimeline> _timeline;
        const IChunkVersionReader&                     _versions;
        RenderCancellation                             _cancellation;

        mutable std::mutex                             _mutex;
        mutable PhraseRenderMetrics                    _metrics;
    };
}

#endif // PHRASE_TILE_RENDERER_HPP