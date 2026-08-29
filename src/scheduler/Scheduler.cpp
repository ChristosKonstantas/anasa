#include "scheduler/Scheduler.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <thread>
#include <cassert>

#include "playback/PlaybackTimeline.hpp"
#include "render/RenderConstants.hpp"
#include "render/RenderFrameUtils.hpp"

namespace anasa
{
    Scheduler::Scheduler(const SchedulerSettings& schedulerSettings, const AudioSettings& audioSettings, const RenderSettings& renderSettings,
                         int totalFrames, SharedState& sharedState, VersionTable& versionTable, Executor& executor)
        : _settings(schedulerSettings),
          _audioBlockFrames(audioSettings.audioBlockFrames),
          _contextFrames(renderSettings.contextFrames),
          _totalFrames(totalFrames),
          _chunkCount(versionTable.count()),
          _sharedState(sharedState),
          _versionTable(versionTable),
          _executor(executor),
          _commandQueue(validateCommandQueueSlots(schedulerSettings.commandQueueSlots)),
          _stopRequested(false),
          _started(false),
          _playRequested(false),
          _viewportFirstFrame(0),
          _viewportLastFrame(std::min(totalFrames, 2 * audioSettings.sampleRate)),
          _nextFrameToPublish(0)
    {

        if (audioSettings.sampleRate <= 0)
            throw std::invalid_argument("sampleRate must be greater than zero");

        if (_audioBlockFrames <= 0)
            throw std::invalid_argument("audioBlockFrames must be greater than zero");

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

        const int expectedChunkCount = (_totalFrames + CHUNK_FRAMES - 1) / CHUNK_FRAMES;

        if (_chunkCount != expectedChunkCount)
            throw std::invalid_argument("VersionTable size does not match timeline chunk count");
    }

    Scheduler::~Scheduler()
    {
        stop();
    }

    void Scheduler::start()
    {
        if (_started)
            return;

        _stopRequested.store(false, std::memory_order_release);
        _playRequested = false;

        // the first sample frame that the audio callback has not consumed yet
        const int nextUnconsumedFrame = _sharedState.nextUnconsumedFrame.load(std::memory_order_acquire);

        // the first frame the scheduler has not yet published scheduler starts without an assumed prebuffer
        _nextFrameToPublish = clampTimelineBoundary(nextUnconsumedFrame, _totalFrames);

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

        while (!_commandQueue.isEmpty())
            _commandQueue.pop();

        _started = false;
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

        while (!_stopRequested.load(std::memory_order_acquire) && !_sharedState.stop.load(std::memory_order_acquire))
        {
            readCommands();

            if (_stopRequested.load(std::memory_order_acquire) || _sharedState.stop.load(std::memory_order_acquire))
                break;

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

            if (_stopRequested.load(std::memory_order_acquire))
                break;
        }
    }

    void Scheduler::handleCommand(Command command)
    {
        if (command.type == CommandType::Play)
        {
            _playRequested = true;
            return;
        }

        if (command.type == CommandType::Pause)
        {
            _playRequested = false;
            _sharedState.playing.store(false, std::memory_order_release);
            return;
        }

        if (command.type == CommandType::Seek)
        {
            const int target = alignFrameToAudioBlock(clampTimelineFrame(command.firstFrame, _totalFrames), _audioBlockFrames);

            _sharedState.playing.store(false, std::memory_order_release);

            // AudioSimulator reads this after observing the new generation.
            _sharedState.targetFrame.store(target, std::memory_order_relaxed);

            // Starts a new published stream.
            _sharedState.generation.fetch_add(1, std::memory_order_acq_rel);

            _sharedState.nextUnconsumedFrame.store(target, std::memory_order_release);

            _nextFrameToPublish = target;
            return;
        }

        if (command.type == CommandType::SetViewport)
        {
            _viewportFirstFrame = clampTimelineFrame(command.firstFrame, _totalFrames);

            _viewportLastFrame = clampTimelineBoundary(std::max(command.firstFrame, command.lastFrame), _totalFrames);

            if (_viewportLastFrame < _viewportFirstFrame)
                _viewportLastFrame = _viewportFirstFrame;

            return;
        }

        if (command.type == CommandType::Edit)
        {
            invalidateVersions(command.firstFrame, command.lastFrame);

            /* Conservatively discard the previously published stream after every edit, including edits performed while paused.*/
            const int target = clampTimelineBoundary(_sharedState.nextUnconsumedFrame.load(std::memory_order_acquire), _totalFrames);

            assert(target % _audioBlockFrames == 0 || target == _totalFrames);

            if (_playRequested && _settings.rebufferOnEdit)
                _sharedState.playing.store(false, std::memory_order_release);

            _sharedState.targetFrame.store(target, std::memory_order_relaxed);
            _sharedState.generation.fetch_add(1, std::memory_order_acq_rel);

            _nextFrameToPublish = target;
            return;
        }

        if (command.type == CommandType::Stop)
        {
            _sharedState.stop.store(true, std::memory_order_release);
            _stopRequested.store(true, std::memory_order_release);
        }
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
            _versionTable.bump(chunk);
    }

    std::size_t Scheduler::validateCommandQueueSlots(int commandQueueSlots)
    {
        if (commandQueueSlots <= 0)
            throw std::invalid_argument("commandQueueSlots must be greater than zero");

        return static_cast<std::size_t>(commandQueueSlots);
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

    bool Scheduler::chunkIntersectsViewport(int chunk) const
    {
        const int chunkFirstFrame = firstFrameOfChunk(chunk);
        const int chunkLastFrame = std::min(chunkFirstFrame + CHUNK_FRAMES, _totalFrames);

        // Both ranges are half-open:
        // chunk:    [chunkFirstFrame, chunkLastFrame)
        // viewport: [_viewportFirstFrame, _viewportLastFrame)
        return chunkFirstFrame < _viewportLastFrame && chunkLastFrame > _viewportFirstFrame;
    }
} // namespace anasa