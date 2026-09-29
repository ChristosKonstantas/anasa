#ifndef PLAYBACK_CONTROLLER_HPP
#define PLAYBACK_CONTROLLER_HPP

#include <atomic>

namespace anasa
{
    // Scheduler-side play intent and buffering decisions. Calls belong to the Scheduler thread, or its owner while stopped.
    class PlaybackController
    {
    public:
        PlaybackController(int prebufferBlocks, bool rebufferOnEdit, std::atomic<bool>& playing);

        bool playRequested() const noexcept;

        // Return whether the play request changed, so Scheduler can reclassify work.
        bool requestPlay() noexcept;
        bool pause() noexcept;

        // Lifecycle only: the owner controls playing during startup/shutdown.
        void resetPlayRequest() noexcept;

        bool shouldRebufferOnEdit() const noexcept;

        // Starts requested playback when ready (never suspends ongoing playback).
        void startIfReady(int readyLeadBlocks, bool entireRemainderPublished) noexcept;

    private:
        const int          _prebufferBlocks;
        const bool         _rebufferOnEdit;
        std::atomic<bool>& _playing;
        bool               _playRequested = false;
    };
} // namespace anasa

#endif // PLAYBACK_CONTROLLER_HPP