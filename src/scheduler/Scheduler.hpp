#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include <atomic>
#include <thread>

#include "audio-pipeline/AudioSettings.hpp"
#include "render/RenderSettings.hpp"
#include "playback/PlaybackState.hpp"
#include "render/VersionTable.hpp"
#include "scheduler/SchedulerSettings.hpp"
#include "scheduler/SchedulerTypes.hpp"
#include "utils/queues/SpscQueue.hpp"

namespace anasa
{
    /*
     * Owns the single Scheduler thread.
     *
     * Threading contract:
     *  - One control thread calls post().
     *  - The Scheduler thread consumes commands.
     *  - start() and stop() are not called concurrently.
     *  - The audio callback never accesses Scheduler directly.
     */
    class Scheduler
    {
    public:
        Scheduler(const SchedulerSettings& schedulerSettings, const AudioSettings& audioSettings, const RenderSettings& renderSettings, 
                  int totalFrames, SharedState& sharedState, VersionTable& versionTable);

        ~Scheduler();

        void                    start();
        void                    stop();
        bool                    post(Command command);

    private:
        void                    schedulerLoop();
        void                    readCommands();
        void                    handleCommand(Command command);
        void                    invalidateVersions(int firstFrame, int lastFrame);
        static std::size_t      validateCommandQueueSlots(int commandQueueSlots);

        const SchedulerSettings _settings;
        const int               _audioBlockFrames;
        const int               _contextFrames;
        const int               _totalFrames;

        SharedState&            _sharedState;
        VersionTable&           _versionTable;

        SpscQueue<Command>      _commandQueue;
        std::thread             _schedulerThread;
        std::atomic<bool>       _stopRequested;
        bool                    _started;

        // Scheduler-thread-owned transport state.
        bool                    _playRequested;
        int                     _viewportFirstFrame;
        int                     _viewportLastFrame;
        int                     _nextFrameToPublish;
    };

} // namespace anasa

#endif // SCHEDULER_HPP