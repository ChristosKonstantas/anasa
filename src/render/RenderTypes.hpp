#ifndef RENDER_TYPES_HPP
#define RENDER_TYPES_HPP

#include <atomic>

#include "audio/AudioBuffer.hpp"
#include "render/RenderConstants.hpp"

namespace anasa
{
    struct RenderJob
    {
        explicit RenderJob(int channelCount)
            : samples(channelCount, CHUNK_FRAMES)
        {}

        int               chunk = 0;
        int               version = 0;
        std::atomic<int>  tilesRemaining{TILES_PER_CHUNK};
        std::atomic<bool> cancelled{false};
        
        // Storage dimensions remain fixed while workers render disjoint frame ranges.
        AudioBuffer samples;
    };

    struct CacheEntry
    {
        explicit CacheEntry(int channelCount)
            : samples(channelCount, CHUNK_FRAMES)
        {}

        int version = 0;
        AudioBuffer samples;
    };
    
} // namespace anasa

#endif // RENDER_TYPES_HPP