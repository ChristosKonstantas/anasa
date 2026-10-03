#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include <atomic>
#include <cstddef>
#include <memory>
#include <queue>
#include <thread>
#include <vector>

#include "audio/AudioSettings.hpp"
#include "audio/AudioTypes.hpp"
#include "execution/IRenderExecutor.hpp"
#include "playback/PlaybackState.hpp"
#include "render/RenderSettings.hpp"
#include "render/RenderTypes.hpp"
#include "render/VersionTable.hpp"
#include "scheduler/SchedulerSettings.hpp"
#include "scheduler/SchedulerTypes.hpp"
#include "scheduler/policies/SchedulingPolicyCompare.hpp"
#include "utils/queues/SpscQueue.hpp"
#include "playback/PlaybackProtocol.hpp"
#include "playback/PlaybackController.hpp"
#include "scheduler/TimelineAudioPublisher.hpp"
#include "scheduler/RenderCache.hpp"

namespace anasa
{
    /*
     * Owns the single Scheduler thread.
     *
     * Threading contract:
     *  - One control thread calls post().
     *  - The Scheduler thread consumes commands.
     *  - The Scheduler thread exclusively owns pending tiles and render cache.
     *  - The Scheduler is the only producer of Executor tasks.
     *  - The Scheduler is the only producer of ready AudioBlocks.
     *  - start() and stop() are not called concurrently.
     *  - The audio callback never accesses Scheduler directly.
     */
    class Scheduler
    {
    public:
        Scheduler(const SchedulerSettings& schedulerSettings, const AudioSettings& audioSettings, const RenderSettings& renderSettings, 
                  int totalFrames, SharedState& sharedState, VersionTable& versionTable, IRenderExecutor& executor, SpscQueue<AudioBlock>& readyAudioQueue);

        ~Scheduler();

        void                                         start();
        void                                         stop();
        bool                                         post(Command command);
                         
    private:
        friend class SchedulerTestRig;

                            /* Main functionality */
                            
        static std::size_t                           validateCommandQueueSlots(int commandQueueSlots);
        static std::vector<PendingRenderTile>        makeReservedTileStorage(int capacity);
        
        void                                         schedulerLoop();
        void                                         readCommands();
        void                                         handleCommand(Command command);
        void                                         collectFinishedJobs();
        void                                         feedAudioQueue();
        void                                         updateBackgroundAdmission();
        void                                         scheduleRenderJobs();
        void                                         refreshPendingClassifications(int playheadFrame);
        RenderClassification                         classifyChunk(int chunk, int playheadFrame) const;
        void                                         scheduleChunk(int chunk, int playheadFrame);
        void                                         dispatchPendingTiles();
        
                            /* Helpers */
        bool                                         shutdownRequested() const;
        void                                         beginAudioGeneration(int targetFrame, bool suspendPlayback);
        void                                         invalidateVersions(int firstFrame, int lastFrame);
        bool                                         cacheIsCurrent(int chunk) const;
        int                                          readyLeadBlocks() const;
        bool                                         chunkIntersectsViewport(int chunk) const;
        int                                          pendingTileLimit(RenderPriority priority) const;

        const SchedulerSettings                      _settings;
        const int                                    _audioBlockFrames;
        const int                                    _channelCount;
        const int                                    _contextFrames;
        const int                                    _totalFrames;
        const int                                    _chunkCount;
             
        SharedState&                                 _sharedState;
        PlaybackProtocol                             _playbackProtocol;
        PlaybackController                           _playbackController;
        VersionTable&                                _versionTable;
        IRenderExecutor&                             _executor;
        TimelineAudioPublisher                       _audioPublisher;             
        SpscQueue<Command>                           _commandQueue;

        std::priority_queue<     
            PendingRenderTile,   
            std::vector<PendingRenderTile>,  
            SchedulingPolicyCompare>                 _pendingTiles;
             
        std::vector<PendingRenderTile>               _reclassificationBuffer;
        RenderCache                                  _cache;
        std::vector<std::shared_ptr<RenderJob>>      _activeJobs;
             
        std::thread                                  _schedulerThread;
        std::atomic<bool>                            _stopRequested;
             
        bool                                         _started;
        bool                                         _backgroundAllowed;
        bool                                         _pendingClassificationsDirty;
             
        int                                          _viewportFirstFrame;
        int                                          _viewportLastFrame;
        int                                          _lastClassifiedPlayheadChunk;
        int                                          _timelineScanCursorInChunks;
             
        long long                                    _nextTileSequence;        
    };

} // namespace anasa

#endif // SCHEDULER_HPP