#ifndef RENDER_FRAME_UTILS_HPP
#define RENDER_FRAME_UTILS_HPP

#include <cassert>

#include "render/RenderConstants.hpp"

namespace anasa
{
    inline int frameToChunk(int frame)
    {
        assert(frame >= 0);

        return frame / CHUNK_FRAMES;
    }

    inline int firstFrameOfChunk(int chunk)
    {
        assert(chunk >= 0);

        return chunk * CHUNK_FRAMES;
    }

} // namespace anasa

#endif // RENDER_FRAME_UTILS_HPP