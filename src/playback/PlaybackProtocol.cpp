#include "playback/PlaybackProtocol.hpp"
#include "playback/PlaybackTimeline.hpp"

#include <cassert>
#include <stdexcept>

namespace anasa
{
    PlaybackProtocol::PlaybackProtocol(int totalFrames, SharedState& sharedState)
        : _totalFrames(totalFrames),
          _sharedState(sharedState)
    {
        if (_totalFrames <= 0)
            throw std::invalid_argument("totalFrames must be greater than zero");
    }

    void PlaybackProtocol::beginGeneration(int targetFrame, bool suspendPlayback) noexcept
    {
        assert(targetFrame >= 0 && targetFrame <= _totalFrames);

        if (suspendPlayback)
            _sharedState.playing.store(false, std::memory_order_release);

        // Publish the target before its generation.
        _sharedState.targetFrame.store(targetFrame, std::memory_order_relaxed);
        _sharedState.generation.fetch_add(1, std::memory_order_acq_rel);
    }

    int PlaybackProtocol::currentFrame() const noexcept
    {
        const int generation = _sharedState.generation.load(std::memory_order_acquire);
        const int cursorGeneration = _sharedState.audioCursorGeneration.load(std::memory_order_acquire);

        // Progress from a previous generation must not override the reset target.
        if (cursorGeneration != generation)
            return clampTimelineBoundary(_sharedState.targetFrame.load(std::memory_order_acquire), _totalFrames);

        return clampTimelineBoundary(_sharedState.nextUnconsumedFrame.load(std::memory_order_acquire), _totalFrames);
    }
} // namespace anasa