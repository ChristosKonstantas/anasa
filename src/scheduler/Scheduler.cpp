#include "scheduler/Scheduler.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "audio-pipeline/AudioConstants.hpp"
#include "playback/PlaybackTimeline.hpp"
#include "render/RenderConstants.hpp"
#include "render/RenderFrameUtils.hpp"
#include "scheduler/policies/SchedulingPolicyFactory.hpp"
namespace anasa
{
    Scheduler::Scheduler(const SchedulerSettings& schedulerSettings, const AudioSettings& audioSettings, const RenderSettings& renderSettings,
                         int totalFrames, SharedState& sharedState, VersionTable& versionTable, Executor& executor, SpscQueue<AudioBlock>& readyAudioQueue)
        : _settings(schedulerSettings),
          _audioBlockFrames(audioSettings.audioBlockFrames),
          _contextFrames(renderSettings.contextFrames),
          _totalFrames(totalFrames),
          _chunkCount(versionTable.count()),
          _sharedState(sharedState),
          _versionTable(versionTable),
          _executor(executor),
          _readyAudioQueue(readyAudioQueue),
          _commandQueue(validateCommandQueueSlots(schedulerSettings.commandQueueSlots)),
          _pendingTiles(SchedulingPolicyCompare(createSchedulingPolicy(schedulerSettings.policyType)), makeReservedTileStorage(_settings.maxPendingTiles)),
          _reclassificationBuffer(makeReservedTileStorage(_settings.maxPendingTiles)),
          _cache(_chunkCount),
          _activeJobs(_chunkCount),
          _stopRequested(false),
          _started(false),
          _playRequested(false),
          _backgroundAllowed(true),
          _pendingClassificationsDirty(true),
          _viewportFirstFrame(0),
          _viewportLastFrame(std::min(totalFrames, 2 * audioSettings.sampleRate)),
          _lastClassifiedPlayheadChunk(-1),
          _nextFrameToPublish(0),
          _timelineScanCursorInChunks(0),
          _nextTileSequence(0)
    {

        if (audioSettings.sampleRate <= 0)
            throw std::invalid_argument("sampleRate must be greater than zero");

        if (_audioBlockFrames <= 0)
            throw std::invalid_argument("audioBlockFrames must be greater than zero");

        if (_audioBlockFrames > MAX_AUDIO_BLOCK_FRAMES)
            throw std::invalid_argument("audioBlockFrames exceeds MAX_AUDIO_BLOCK_FRAMES");

        if (CHUNK_FRAMES % _audioBlockFrames != 0)
            throw std::invalid_argument("audioBlockFrames must divide CHUNK_FRAMES");

        if (_contextFrames < 0)
            throw std::invalid_argument("contextFrames must not be negative");

        if (_totalFrames <= 0)
            throw std::invalid_argument("totalFrames must be greater than zero");

        if (_totalFrames % _audioBlockFrames != 0)
            throw std::invalid_argument("totalFrames must be divisible by audioBlockFrames - current engine does not support a partial final audio block");

        if (_settings.prebufferBlocks < 0)
            throw std::invalid_argument("prebufferBlocks must not be negative");

        if (_settings.lowWaterBlocks < 0)
            throw std::invalid_argument("lowWaterBlocks must not be negative");

        if (_settings.highWaterBlocks < _settings.lowWaterBlocks)
            throw std::invalid_argument("highWaterBlocks must not be smaller than lowWaterBlocks");

        if (_settings.urgentChunks <= 0)
            throw std::invalid_argument("urgentChunks must be greater than zero");

        if (_settings.maxPendingTiles <= 0)
            throw std::invalid_argument("maxPendingTiles must be greater than zero");

        if (_settings.maxPendingTiles < TILES_PER_CHUNK)
            throw std::invalid_argument("maxPendingTiles must hold at least one complete chunk");

        if (_settings.urgentReservedTiles < 0 || _settings.urgentReservedTiles > _settings.maxPendingTiles)
            throw std::invalid_argument("urgentReservedTiles must be between zero and maxPendingTiles");
        
        const int readyQueueCapacity = static_cast<int>(_readyAudioQueue.capacity());

        if (_settings.prebufferBlocks > readyQueueCapacity)
            throw std::invalid_argument("prebufferBlocks exceeds ready-audio queue capacity");
        
        if (_settings.highWaterBlocks > readyQueueCapacity)
            throw std::invalid_argument("highWaterBlocks exceeds ready-audio queue capacity");

        if (_settings.urgentReservedTiles < TILES_PER_CHUNK)
            throw std::invalid_argument("urgentReservedTiles must hold one complete chunk");

        const int expectedChunkCount = (_totalFrames + CHUNK_FRAMES - 1) / CHUNK_FRAMES;

        if (_chunkCount != expectedChunkCount)
            throw std::invalid_argument("VersionTable size does not match timeline chunk count");
    }

    Scheduler::~Scheduler()
    {
        stop();
    }

    std::size_t Scheduler::validateCommandQueueSlots(int commandQueueSlots)
    {
        if (commandQueueSlots <= 0)
            throw std::invalid_argument("commandQueueSlots must be greater than zero");

        return static_cast<std::size_t>(commandQueueSlots);
    }

    std::vector<PendingRenderTile> Scheduler::makeReservedTileStorage(int capacity)
    {
        if (capacity <= 0)
            throw std::invalid_argument("capacity must be greater than zero");

        std::vector<PendingRenderTile> storage;
        storage.reserve(static_cast<std::size_t>(capacity));
        return storage;
    }

    void Scheduler::start()
    {
        if (_started)
            return;

        _stopRequested.store(false, std::memory_order_release);
        _playRequested = false;
        _backgroundAllowed = true;
        _pendingClassificationsDirty = true;
        _lastClassifiedPlayheadChunk = -1;
        _nextFrameToPublish = currentPlaybackFrame();

        assert(_nextFrameToPublish % _audioBlockFrames == 0 || _nextFrameToPublish == _totalFrames);

        _started = true;

        try
        {
            _schedulerThread = std::thread([this]{schedulerLoop();});
        }
        catch (...)
        {
            _stopRequested.store(true, std::memory_order_release);
            _started = false;
            throw;
        }
    }

    void Scheduler::stop()
    {
        if (!_started)
            return;

        _stopRequested.store(true, std::memory_order_release);

        if (_schedulerThread.joinable())
            _schedulerThread.join();

        while (!_pendingTiles.empty())
        {
            const PendingRenderTile& tile = _pendingTiles.top();

            if (tile.job != nullptr)
                tile.job->cancelled.store(true, std::memory_order_relaxed);

            _pendingTiles.pop();
        }
        
        while (!_commandQueue.isEmpty())
            _commandQueue.pop();

        for (std::shared_ptr<RenderJob>& job : _activeJobs)
        {
            if (job != nullptr)
            {
                job->cancelled.store(true, std::memory_order_relaxed);
                job.reset();
            }
        }

        _playRequested = false;
        _backgroundAllowed = true;
        _timelineScanCursorInChunks = 0;
        _nextTileSequence = 0;
        _started = false;
        _pendingClassificationsDirty = true;
        _lastClassifiedPlayheadChunk = -1;
    }

    bool Scheduler::post(Command command)
    {
        if (!_started)
            return false;

        if (_stopRequested.load(std::memory_order_acquire))
            return false;

        if (_sharedState.stop.load(std::memory_order_acquire))
            return false;

        return _commandQueue.pushWith([command](Command& destination){destination = command;});
    }

    void Scheduler::schedulerLoop()
    {
        using namespace std::chrono_literals;

        while (!shutdownRequested())
        {
            readCommands();

            if (shutdownRequested())
                break;
            
            collectFinishedJobs();
            feedAudioQueue();
            updateBackgroundAdmission();
            scheduleRenderJobs();
            dispatchPendingTiles();

            // TODO: make this event-driven
            std::this_thread::sleep_for(1ms);
        }
    }

    void Scheduler::readCommands()
    {
        while (true)
        {
            const Command* queuedCommand = _commandQueue.front();

            if (queuedCommand == nullptr)
                break;

            Command command = *queuedCommand;
            _commandQueue.pop();

            handleCommand(command);

            if (shutdownRequested())
                break;
        }
    }

    void Scheduler::handleCommand(Command command)
    {
        switch (command.type)
        {
            case CommandType::Play:
            {
                if (!_playRequested)
                    _pendingClassificationsDirty = true;
                
                _playRequested = true;
                
                break;
            }

            case CommandType::Pause:
            {
                if (_playRequested)
                    _pendingClassificationsDirty = true;

                _playRequested = false;
                _sharedState.playing.store(false, std::memory_order_release);
                
                break;
            }

            case CommandType::Seek:
            {
                const int target = alignFrameToAudioBlock(clampTimelineFrame(command.firstFrame, _totalFrames), _audioBlockFrames);

                beginAudioGeneration(target, true);

                _pendingClassificationsDirty = true;

                break;
            }

            case CommandType::SetViewport:
            {
                _viewportFirstFrame = clampTimelineFrame(command.firstFrame, _totalFrames);

                _viewportLastFrame = clampTimelineBoundary(std::max(command.firstFrame, command.lastFrame), _totalFrames);

                _pendingClassificationsDirty = true;
                break;
            }

            case CommandType::Edit:
            {
                invalidateVersions(command.firstFrame, command.lastFrame);

                beginAudioGeneration(currentPlaybackFrame(), _playRequested && _settings.rebufferOnEdit);
                break;
            }

            case CommandType::Stop:
            {
                _sharedState.stop.store(true, std::memory_order_release);
                _stopRequested.store(true, std::memory_order_release);
                break;
            }
        }
    }

    void Scheduler::collectFinishedJobs()
    {
        std::shared_ptr<RenderJob> job;

        while (_executor.popCompleted(job))
        {
            if (job == nullptr)
                continue;

            const int chunk = job->chunk;

            if (chunk < 0 || chunk >= _chunkCount)
                continue;

            std::shared_ptr<RenderJob>& activeJob = _activeJobs[chunk];

            // Ignore completion from a job that has already been replaced.
            if (activeJob != job)
                continue;

            if (job->cancelled.load(std::memory_order_relaxed) || _versionTable.get(chunk) != job->version)
            {
                activeJob.reset();
                continue;
            }

            _cache[chunk].version = job->version;
            _cache[chunk].samples = job->samples;

            activeJob.reset();
        }
    }

    void Scheduler::feedAudioQueue()
    {
        const int nextUnconsumedFrame = currentPlaybackFrame();

        if (nextUnconsumedFrame >= _totalFrames)
        {
            _nextFrameToPublish = _totalFrames;
            
            if (_playRequested)
                _pendingClassificationsDirty = true;

            _playRequested = false;
            _sharedState.playing.store(false, std::memory_order_release);
            return;
        }

        // If playback already passed unpublished audio, late audio is useless.
        if (_nextFrameToPublish < nextUnconsumedFrame)
            _nextFrameToPublish = nextUnconsumedFrame;

        const int generation = _sharedState.generation.load(std::memory_order_acquire);

        while (_nextFrameToPublish + _audioBlockFrames <= _totalFrames)
        {
            const int chunk = frameToChunk(_nextFrameToPublish);

            // Publication is strictly ordered. Never skip a missing chunk. Postpone publication until the required chunk becomes available.
            if (!cacheIsCurrent(chunk))
                break;

            const int blockFirstFrame = _nextFrameToPublish; // remember it's aligned to first frame of the audio block (see Seek)
            const int chunkOffset = _nextFrameToPublish - firstFrameOfChunk(chunk); // locate the block's first frame within the chunk

            assert(chunkOffset >= 0);
            assert(chunkOffset + _audioBlockFrames <= CHUNK_FRAMES);

            const bool pushed = _readyAudioQueue.pushWith([this, generation, blockFirstFrame, chunk, chunkOffset](AudioBlock& block)
            {
                block.generation = generation;
                block.firstFrame = blockFirstFrame;
                block.frameCount = _audioBlockFrames;

                for (int frame = 0; frame < _audioBlockFrames; ++frame)
                    block.samples[frame] = _cache[chunk].samples[chunkOffset + frame];
            });

            if (!pushed)
                break;

            _nextFrameToPublish += _audioBlockFrames;
        }

        if (!_playRequested ||  _sharedState.playing.load(std::memory_order_acquire))
            return;

        // below now is executed only if (_playRequested && !_sharedState.playing.load(std::memory_order_acquire))
        // play is requested but playing has not started ->> prebuffering
        const bool prebufferReady = readyLeadBlocks() >= _settings.prebufferBlocks;
        const bool entireRemainderPublished = _nextFrameToPublish >= _totalFrames;

        if (prebufferReady || entireRemainderPublished)
            _sharedState.playing.store(true, std::memory_order_release); // now callback is allowed to advance
    }

    void Scheduler::updateBackgroundAdmission()
    {
        if (_settings.policyType == SchedulingPolicyType::Fifo || !_playRequested)
        {
            _backgroundAllowed = true;
            return;
        }

        const int leadBlocks = readyLeadBlocks();

        if (leadBlocks < _settings.lowWaterBlocks)
            _backgroundAllowed = false;
        else if (leadBlocks >= _settings.highWaterBlocks)
            _backgroundAllowed = true;
    }

    void Scheduler::scheduleRenderJobs()
    {
        const int playheadFrame = currentPlaybackFrame();

        const int playheadChunk = playheadFrame < _totalFrames ? frameToChunk(playheadFrame) : _chunkCount;

        // Rebuild only when priorities may have materially changed.
        const bool classificationChanged = _pendingClassificationsDirty || playheadChunk != _lastClassifiedPlayheadChunk;

        // Refresh priority, deadline and distance for _pendingTiles. This is required for Priority scheduling, not for FIFO scheduling.
        if (_settings.policyType == SchedulingPolicyType::Priority && classificationChanged)
            refreshPendingClassifications(playheadFrame);

        _pendingClassificationsDirty = false;
        _lastClassifiedPlayheadChunk = playheadChunk;

        // (1) Playback-near chunks (urgent).
        if (_playRequested && playheadFrame < _totalFrames)
        {
            const int firstChunk = frameToChunk(playheadFrame);
            const int lastChunk = std::min(firstChunk + _settings.urgentChunks, _chunkCount);

            for (int chunk = firstChunk; chunk < lastChunk; ++chunk)
                scheduleChunk(chunk, playheadFrame);
        }

        // (2) Chunks intersecting the viewport. Urgent classification still takes precedence.
        if (_viewportLastFrame > _viewportFirstFrame)
        {
            const int firstChunk = frameToChunk(_viewportFirstFrame);

            const int lastVisibleFrame = std::min(_viewportLastFrame - 1, _totalFrames - 1);

            const int lastChunk = std::min(frameToChunk(lastVisibleFrame) + 1, _chunkCount);

            for (int chunk = firstChunk; chunk < lastChunk; ++chunk)
                scheduleChunk(chunk, playheadFrame);
        }

        // (3) Scan the timeline round-robin to fill non-urgent queue capacity.
        
        // Non-urgent work may be added only while the total pending size remains below this ceiling.
        const int nonUrgentLimitInTiles = pendingTileLimit(RenderPriority::Background);

        // The number of chunks to check for background work is limited to avoid spending too much time scanning the entire timeline.
        const int backgroundCheckLimitInChunks = std::min(_chunkCount, _settings.maxPendingTiles / TILES_PER_CHUNK);

        int checkedChunks = 0;

        // Loop while: 
        // 1. There is enough reserved non-urgent capacity for one complete chunk.
        // 2. The number of chunks checked does not exceed the configured limit.
        while (static_cast<int>(_pendingTiles.size()) + TILES_PER_CHUNK <= nonUrgentLimitInTiles && checkedChunks < backgroundCheckLimitInChunks)
        {
            // scheduleChunk() already classifies the chunk.
            scheduleChunk(_timelineScanCursorInChunks, playheadFrame);

            _timelineScanCursorInChunks = (_timelineScanCursorInChunks + 1) % _chunkCount;

            ++checkedChunks;
        }
    }

    void Scheduler::refreshPendingClassifications(int playheadFrame)
    {
        _reclassificationBuffer.clear();

        const RenderJob* classifiedJob = nullptr;
        RenderClassification classification;

        while (!_pendingTiles.empty())
        {
            PendingRenderTile tile = _pendingTiles.top();
            _pendingTiles.pop();

            if (tile.job.get() != classifiedJob)
            {
                classifiedJob = tile.job.get();
                classification = classifyChunk(tile.job->chunk, playheadFrame);
            }

            tile.classification = classification;
            _reclassificationBuffer.push_back(std::move(tile));
        }

        for (PendingRenderTile& tile : _reclassificationBuffer)
            _pendingTiles.push(std::move(tile));
    }

    RenderClassification Scheduler::classifyChunk(int chunk, int playheadFrame) const
    {
        if (chunk < 0 || chunk >= _chunkCount)
            throw std::out_of_range("chunk index is outside the timeline");

        const int boundedPlayhead = clampTimelineBoundary(playheadFrame, _totalFrames);
        const int chunkFirstFrame = firstFrameOfChunk(chunk);

        RenderClassification classification;
        classification.distanceInFrames = std::abs(chunkFirstFrame - boundedPlayhead);

        // _playRequested usage instead of SharedState::playing because urgent prebuffering must happen *before* playback is allowed to begin.
        if (_playRequested && boundedPlayhead < _totalFrames)
        {
            const int firstUrgentChunk = frameToChunk(boundedPlayhead);
            const int lastUrgentChunk = std::min(firstUrgentChunk + _settings.urgentChunks, _chunkCount);

            if (chunk >= firstUrgentChunk && chunk < lastUrgentChunk)
            {
                classification.priority = RenderPriority::Urgent;

                // The chunk containing the playhead is required immediately. Later chunks are required when playback reaches their start.
                classification.deadlineFrame = chunkFirstFrame;

                return classification;
            }
        }

        if (chunkIntersectsViewport(chunk))
        {
            classification.priority = RenderPriority::Visible;
            return classification;
        }

        classification.priority = RenderPriority::Background;
        return classification;
    }
    
    void Scheduler::scheduleChunk(int chunk, int playheadFrame)
    {
        if (chunk < 0 || chunk >= _chunkCount)
            return;

        if (cacheIsCurrent(chunk))
            return;

        const int version = _versionTable.get(chunk);

        std::shared_ptr<RenderJob>& currentJob = _activeJobs[chunk];
        
        // this is well scheduled already, therefore no need to schedule again (not nullptr, version is the same, not cancelled)
        if (currentJob != nullptr && currentJob->version == version && !currentJob->cancelled.load(std::memory_order_relaxed))
            return;

        const RenderClassification classification = classifyChunk(chunk, playheadFrame);
        const int queueLimit = pendingTileLimit(classification.priority);

        if (static_cast<int>(_pendingTiles.size()) + TILES_PER_CHUNK > queueLimit)
            return;

        if (currentJob != nullptr) // cancelled or version is different
            currentJob->cancelled.store(true, std::memory_order_relaxed);

        // now, activeJob will be replaced with a new job below
        std::shared_ptr<RenderJob> jobToSchedule = std::make_shared<RenderJob>();

        jobToSchedule->chunk = chunk;
        jobToSchedule->version = version;
        jobToSchedule->cancelled.store(false, std::memory_order_relaxed); // the only false store as it initializes a newly created unpublished job
        jobToSchedule->tilesRemaining.store(TILES_PER_CHUNK, std::memory_order_relaxed);

        for (int tileIndex = 0; tileIndex < TILES_PER_CHUNK; ++tileIndex)
        {
            PendingRenderTile tile;

            tile.job = jobToSchedule;
            tile.tileIndex = tileIndex;
            tile.classification = classification;
            tile.sequence = _nextTileSequence++;

            _pendingTiles.push(std::move(tile));
        }

        currentJob = std::move(jobToSchedule);
    }

    void Scheduler::dispatchPendingTiles()
    {
        while (!_pendingTiles.empty() && _executor.queuedTaskCount() < _executor.workerCount())
        {
            const PendingRenderTile& tile = _pendingTiles.top();

            if (!_backgroundAllowed && tile.classification.priority == RenderPriority::Background)
                return;

            RenderTask task;
            task.job = tile.job;
            task.tileIndex = tile.tileIndex;

            if (!_executor.submit(std::move(task)))
                return;

            _pendingTiles.pop();
        }
    }

    /* ------------------------------------------- Helpers ------------------------------------------------------------*/
    bool Scheduler::shutdownRequested() const
    {
        return _stopRequested.load(std::memory_order_acquire) || _sharedState.stop.load(std::memory_order_acquire);
    }

    void Scheduler::beginAudioGeneration(int targetFrame, bool suspendPlayback)
    {
        assert(targetFrame % _audioBlockFrames == 0 || targetFrame == _totalFrames);

        if (suspendPlayback)
            _sharedState.playing.store(false, std::memory_order_release);

        // targetFrame must be published before generation.
        _sharedState.targetFrame.store(targetFrame, std::memory_order_relaxed);

        _sharedState.generation.fetch_add(1, std::memory_order_acq_rel);

        _nextFrameToPublish = targetFrame;
    }

    int Scheduler::currentPlaybackFrame() const
    {
        // Before audio thread acknowledges seek -> Scheduler follows targetFrame
        // After audio thread acknowledges seek  -> Scheduler follows nextUnconsumedFrame
        const int generation = _sharedState.generation.load(std::memory_order_acquire);
        const int cursorGeneration = _sharedState.audioCursorGeneration.load(std::memory_order_acquire);

        // The audio cursor still belongs to the previous stream.
        if (cursorGeneration != generation)
            return clampTimelineBoundary(_sharedState.targetFrame.load(std::memory_order_acquire), _totalFrames);

        return clampTimelineBoundary(_sharedState.nextUnconsumedFrame.load(std::memory_order_acquire), _totalFrames);
    }

    void Scheduler::invalidateVersions(int firstFrame, int lastFrame)
    {
        firstFrame = clampTimelineFrame(firstFrame, _totalFrames);
        lastFrame = clampTimelineFrame(std::max(firstFrame, lastFrame), _totalFrames);

        const int haloFirstFrame = std::max(0, firstFrame - _contextFrames);
        const int haloLastFrame = std::min(_totalFrames - 1, lastFrame + _contextFrames);

        const int firstChunk = frameToChunk(haloFirstFrame);
        const int lastChunk = frameToChunk(haloLastFrame);

        for (int chunk = firstChunk; chunk <= lastChunk; ++chunk)
        {
            _versionTable.bump(chunk);

            if (_activeJobs[chunk] != nullptr)
                _activeJobs[chunk]->cancelled.store(true, std::memory_order_relaxed);
        }
    }

    bool Scheduler::cacheIsCurrent(int chunk) const
    {
        return _cache[chunk].version == _versionTable.get(chunk);
    }

    int Scheduler::readyLeadBlocks() const
    {
        const int nextUnconsumedFrame = currentPlaybackFrame();

        const int readyFrames = std::max(0, _nextFrameToPublish - nextUnconsumedFrame);

        return readyFrames / _audioBlockFrames;
    }

    bool Scheduler::chunkIntersectsViewport(int chunk) const
    {
        const int chunkFirstFrame = firstFrameOfChunk(chunk);
        const int chunkLastFrame = std::min(chunkFirstFrame + CHUNK_FRAMES, _totalFrames);

        // Both ranges are half-open:
        // chunk:    [chunkFirstFrame, chunkLastFrame)
        // viewport: [_viewportFirstFrame, _viewportLastFrame)
        return chunkFirstFrame < _viewportLastFrame && chunkLastFrame > _viewportFirstFrame;
    }

    int Scheduler::pendingTileLimit(RenderPriority priority) const
    {
        if (priority == RenderPriority::Urgent)
            return _settings.maxPendingTiles;

        return _settings.maxPendingTiles - _settings.urgentReservedTiles;
    }

} // namespace anasa