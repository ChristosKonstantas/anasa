#ifndef RENDER_CACHE_HPP
#define RENDER_CACHE_HPP

#include <vector>

#include "render/IChunkVersionReader.hpp"
#include "render/RenderTypes.hpp"
#include "scheduler/IRenderCacheReader.hpp"

namespace anasa
{
    // Scheduler-thread only, or its owner while stopped. versions must outlive the cache.
    class RenderCache final : public IRenderCacheReader
    {
    public:
        RenderCache(int channelCount, int totalFrames, const IChunkVersionReader& versions);

        int chunkCount() const noexcept override;
        const AudioBuffer* findCurrent(int chunk) const override;

        bool store(int chunk, int version, const AudioBuffer& samples);

    private:
        const int                  _channelCount;
        const IChunkVersionReader& _versions;
        std::vector<CacheEntry>    _cacheEntries;
    };
} // namespace anasa

#endif // RENDER_CACHE_HPP