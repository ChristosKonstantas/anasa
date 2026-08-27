#ifndef PLAYBACK_STATE_HPP
#define PLAYBACK_STATE_HPP

#include <atomic>

namespace anasa
{
    /* -----------------------------------------------------------------*/
    /*                Small cross-thread playback state.                */
    /* -----------------------------------------------------------------*/
    /* Scheduler writes transport and stream-reset information.         */
    /* AudioSimulator reads it and publishes playback progress.         */ 
    /*                                                                  */ 
    /* This contains only atomics because Scheduler and AudioSimulator  */
    /* access these values concurrently without a mutex.                */
    /*------------------------------------------------------------------*/
    struct SharedState
    {
        // Global engine-shutdown request. If true, engine threads must exit.
        std::atomic<bool> stop{false};

        // Controls whether the audio callback consumes blocks and advances the playback position. 
        // It is false while paused or rebuffering.
        std::atomic<bool> playing{false};

        /* Identifies the currently published audio stream. */
        // Seek and Edit increment it after establishing a new targetFrame.
        // AudioBlocks carry the generation that produced them, allowing the audio callback to reject blocks from an older stream.
        // This is *not* the same as a render-content version:
        //  - generation protects the published playback stream.
        //  - VersionTable protects rendered chunk contents.
        std::atomic<int> generation{1}; // increments by 1 when new stream begins at the seek target

        /* First frame expected when the current generation begins. */
        // It must be a valid audio-block boundary.
        // The Scheduler changes it during Seek or Edit before incrementing generation. 
        // The audio callback reads it when it detects that change.
        std::atomic<int> targetFrame{0};

        /* Consumer cursor: first timeline frame not yet consumed by the audio callback. */
        // After consuming or missing a n-frame block, the callback advances this value by n.
        // The Scheduler reads it to determine the current: playhead, buffer lead and where stream publication should resume.
        // During Seek or Edit, the Scheduler resets it to targetFrame.
        std::atomic<int> nextUnconsumedFrame{0};
    };

} // namespace anasa

#endif // PLAYBACK_STATE_HPP