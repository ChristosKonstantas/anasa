#include "playback/BackgroundRenderGate.hpp"

#include <stdexcept>

namespace anasa
{
    BackgroundRenderGate::BackgroundRenderGate(int lowWaterBlocks, int highWaterBlocks)
        : _lowWaterBlocks(lowWaterBlocks),
          _highWaterBlocks(highWaterBlocks)
    {
        if (lowWaterBlocks < 0)
            throw std::invalid_argument("lowWaterBlocks must not be negative");

        if (highWaterBlocks < 0)
            throw std::invalid_argument("highWaterBlocks must not be negative");
        
        if (highWaterBlocks < lowWaterBlocks)
            throw std::invalid_argument("highWaterBlocks must not be smaller than lowWaterBlocks");
    }

    void BackgroundRenderGate::observe(int readyLeadBlocks) noexcept
    {
        if (readyLeadBlocks < _lowWaterBlocks)
            _allowed = false;
        else if (readyLeadBlocks >= _highWaterBlocks)
            _allowed = true;
    }

    void BackgroundRenderGate::reset() noexcept
    {
        _allowed = true;
    }

    bool BackgroundRenderGate::allowsBackground() const noexcept
    {
        return _allowed;
    }
} // namespace anasa