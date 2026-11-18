#ifndef RENDER_LAYOUT_HPP
#define RENDER_LAYOUT_HPP

#include "render/RenderConstants.hpp"
#include <cassert>
#include <limits>
#include <stdexcept>

namespace anasa
{
    class RenderLayout
    {
    public:
        RenderLayout() = default;

        RenderLayout(int chunkFrames, int tileFrames)
            : _chunkFrames(chunkFrames), _tileFrames(tileFrames)
        {
            if (_chunkFrames <= 0 || _tileFrames <= 0 ||
                _chunkFrames % _tileFrames != 0)
            {
                throw std::invalid_argument(
                    "Render chunks must contain a positive integral number of tiles");
            }
        }

        int chunkFrames() const noexcept { return _chunkFrames; }
        int tileFrames() const noexcept { return _tileFrames; }
        int tilesPerChunk() const noexcept { return _chunkFrames / _tileFrames; }

        int chunkCount(int totalFrames) const
        {
            if (totalFrames <= 0)
                throw std::invalid_argument("Timeline must contain frames");

            return 1 + (totalFrames - 1) / _chunkFrames;
        }

        int frameToChunk(int frame) const noexcept
        {
            assert(frame >= 0);
            return frame / _chunkFrames;
        }

        int firstFrameOfChunk(int chunk) const
        {
            if (chunk < 0 ||
                chunk > std::numeric_limits<int>::max() / _chunkFrames)
            {
                throw std::out_of_range(
                    "Chunk position exceeds the supported frame range");
            }

            return chunk * _chunkFrames;
        }

    private:
        int _chunkFrames = CHUNK_FRAMES;
        int _tileFrames = TILE_FRAMES;
    };
}

#endif // RENDER_LAYOUT_HPP