#ifndef RENDER_TYPES_HPP
#define RENDER_TYPES_HPP

#include <atomic>
#include <array>

#include "render/RenderConstants.hpp"

namespace anasa
{
    struct RenderJob
    {
        int chunk = 0;
        int version = 0;
        std::atomic<int> tilesRemaining{TILES_PER_CHUNK};
        std::atomic<bool> cancelled{false};
        std::array<float, CHUNK_FRAMES> samples{};
    };
    
    struct CacheEntry
    {
        int version = 0;
        bool ready = false;
        std::array<float, CHUNK_FRAMES> samples{};
    };

} // namespace anasa

#endif // RENDER_TYPES_HPP