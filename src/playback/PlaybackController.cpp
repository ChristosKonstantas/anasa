#include "playback/PlaybackController.hpp"

#include <cassert>
#include <stdexcept>

namespace anasa
{
    PlaybackController::PlaybackController(int prebufferBlocks, bool rebufferOnEdit, std::atomic<bool>& playing)
        : _prebufferBlocks(prebufferBlocks),
          _rebufferOnEdit(rebufferOnEdit),
          _playing(playing)
    {
        if (_prebufferBlocks < 0)
            throw std::invalid_argument("prebufferBlocks must not be negative");
    }

    bool PlaybackController::playRequested() const noexcept
    {
        return _playRequested;
    }

    bool PlaybackController::requestPlay() noexcept
    {
        const bool changed = !_playRequested;
        _playRequested = true;
        return changed;
    }

    bool PlaybackController::pause() noexcept
    {
        const bool changed = _playRequested;
        _playRequested = false;
        _playing.store(false, std::memory_order_release);
        return changed;
    }

    void PlaybackController::resetPlayRequest() noexcept
    {
        _playRequested = false;
    }

    bool PlaybackController::shouldRebufferOnEdit() const noexcept
    {
        return _playRequested && _rebufferOnEdit;
    }

    void PlaybackController::startIfReady(int readyLeadBlocks, bool entireRemainderPublished) noexcept
    {
        assert(readyLeadBlocks >= 0);

        if (!_playRequested || _playing.load(std::memory_order_acquire))
            return;

        if (readyLeadBlocks >= _prebufferBlocks || entireRemainderPublished)
            _playing.store(true, std::memory_order_release);
    }
} // namespace anasa