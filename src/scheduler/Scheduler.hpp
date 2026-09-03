#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include <atomic>
#include <cstddef>
#include <memory>
#include <queue>
#include <thread>
#include <vector>

#include "audio-pipeline/AudioSettings.hpp"
#include "audio-pipeline/AudioTypes.hpp"
#include "execution/Executor.hpp"
#include "playback/PlaybackState.hpp"
#include "render/RenderSettings.hpp"
#include "render/RenderTypes.hpp"
#include "render/VersionTable.hpp"
#include "scheduler/SchedulerSettings.hpp"
#include "scheduler/SchedulerTypes.hpp"
#include "scheduler/policies/ISchedulingPolicy.hpp"
#include "scheduler/policies/SchedulingPolicyCompare.hpp"
#include "utils/queues/SpscQueue.hpp"

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
                  int totalFrames, SharedState& sharedState, VersionTable& versionTable, Executor& executor, SpscQueue<AudioBlock>& readyAudioQueue);

        ~Scheduler();

        void                                     start();
        void                                     stop();
        bool                                     post(Command command);
                         
    private:          
        static std::size_t                       validateCommandQueueSlots(int commandQueueSlots);       
        void                                     schedulerLoop();
        void                                     readCommands();
        void                                     handleCommand(Command command);
        void                                     invalidateVersions(int firstFrame, int lastFrame);
        void                                     collectFinishedJobs();
        void                                     feedAudioQueue();
        bool                                     cacheIsCurrent(int chunk) const;
        RenderClassification                     classifyChunk(int chunk, int playheadFrame) const;
        bool                                     chunkIntersectsViewport(int chunk) const;
        void                                     scheduleRenderJobs();
        void                                     scheduleChunk(int chunk, int playheadFrame);
        void                                     refreshPendingClassifications(int playheadFrame);
        int                                      pendingTileLimit(RenderPriority priority) const;
        void                                     updateBackgroundAdmission();
        void                                     dispatchPendingTiles();
        int                                      currentPlaybackFrame() const;
        int                                      readyLeadBlocks() const;

        const SchedulerSettings                  _settings;
        const int                                _audioBlockFrames;
        const int                                _contextFrames;
        const int                                _totalFrames;
        const int                                _chunkCount;
        
        SharedState&                             _sharedState;
        VersionTable&                            _versionTable;
        Executor&                                _executor;
        SpscQueue<AudioBlock>&                   _readyAudioQueue;

        SpscQueue<Command>                       _commandQueue;
        std::shared_ptr<const ISchedulingPolicy> _schedulingPolicy;

        std::priority_queue<
            PendingRenderTile,
            std::vector<PendingRenderTile>,
            SchedulingPolicyCompare>             _pendingTiles;
        std::vector<CacheEntry>                  _cache;
        std::vector<std::shared_ptr<RenderJob>>  _activeJobs;
        
        std::thread                              _schedulerThread;
        std::atomic<bool>                        _stopRequested;
        
        bool                                     _started;
        bool                                     _playRequested;
        bool                                     _backgroundAllowed;
        bool                                     _pendingClassificationsDirty;
        
        int                                      _viewportFirstFrame;
        int                                      _viewportLastFrame;
        int                                      _lastClassifiedPlayheadChunk;
        int                                      _nextFrameToPublish;
        int                                      _timelineScanCursor;

        long long                                _nextTileSequence;        
    };

} // namespace anasa

#endif // SCHEDULER_HPP