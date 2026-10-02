#ifndef PLAYBACK_PROTOCOL_HPP
#define PLAYBACK_PROTOCOL_HPP

#include "playback/PlaybackState.hpp"

namespace anasa
{
    // Scheduler-side generation changes and playback-position resolution.
    // Calls belong to the Scheduler thread, or its owner while stopped.
    class PlaybackProtocol
    {
    public:
        // sharedState must outlive this object. totalFrames must be positive.
        PlaybackProtocol(int totalFrames, SharedState& sharedState);

        // Caller supplies an audio-block boundary in [0, totalFrames].
        // Does not change the consumer cursor or its acknowledgement.
        void beginGeneration(int targetFrame, bool suspendPlayback) noexcept;

        // Uses the reset target until the consumer acknowledges this generation.
        int currentFrame() const noexcept;

    private:
        const int    _totalFrames;
        SharedState& _sharedState;
    };
} // namespace anasa

#endif // PLAYBACK_PROTOCOL_HPP