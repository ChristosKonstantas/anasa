#include <limits>
#include <stdexcept>

#include "Engine.hpp"
#include "render/Renderer.hpp"
#include "render/kernels/SyntheticRenderKernel.hpp"

namespace anasa
{

    Engine::Engine(EngineSettings settings, RendererFactory rendererFactory)
        :_settings(settings),
         _totalFrames(calculateTotalFrames(_settings)),
         _chunkCount(_settings.render.layout.chunkCount(_totalFrames)),
         _versionTable(_chunkCount),
         _sharedState(),
         _readyAudioQueue(READY_AUDIO_QUEUE_SLOTS, _settings.audio.channelCount, _settings.audio.audioBlockFrames),
         _renderer(createRenderer(rendererFactory, _settings.audio, _settings.render, _versionTable)),
         _executor(_settings.executor, *_renderer),
         _scheduler(_settings.scheduler, _settings.audio, _settings.render, _totalFrames, _sharedState, _versionTable, _executor, _readyAudioQueue),
         _audioBlockProcessor(_settings.audio.audioBlockFrames, _totalFrames, _sharedState, _readyAudioQueue),
         _audioSimulator(_settings.audio, _sharedState.stop, _audioBlockProcessor, _settings.captureFrames),
         _started(false)
    {
    }

    std::unique_ptr<ITileRenderer> Engine::createRenderer(const RendererFactory& factory,
        const AudioSettings& audio, const RenderSettings& render, const IChunkVersionReader& versions)
    {
        if (!factory)
            return std::make_unique<Renderer>(std::make_unique<SyntheticRenderKernel>(audio.sampleRate, render.workIterations), versions);

        std::unique_ptr<ITileRenderer> renderer = factory(audio, render, versions);

        if (!renderer)
            throw std::invalid_argument("Renderer factory must return a non-null renderer");

        return renderer;
    }

    Engine::~Engine()
    {
        stop();
    }
    
    void Engine::start()
    {
        if (_started)
            return;
            
        _readyAudioQueue.reset();
        _sharedState.stop.store(false, std::memory_order_release);
        _sharedState.playing.store(false, std::memory_order_release);

        try
        {
            // start dependencies before their future producer/consumer pipeline.
            // Executor must exist before Scheduler starts dispatching.
            // Scheduler must exist before AudioSimulator starts consuming.
            _executor.start();
            _scheduler.start();
            _audioSimulator.start();
        }
        catch (...)
        {
            // clean up if either component fails to start..
            _sharedState.stop.store(true, std::memory_order_release);
            _sharedState.playing.store(false, std::memory_order_release);
            
            _scheduler.stop();
            _audioSimulator.stop();
            _executor.stop();

            throw;
        }

        _started = true;
    }

    void Engine::stop()
    {
        if (!_started)
            return;

            // publish the global engine shutdown request first.
            _sharedState.stop.store(true, std::memory_order_release);
            _sharedState.playing.store(false, std::memory_order_release);
            
            // stop Scheduler first so it cannot produce more Executor tasks or ready-audio blocks.
            _scheduler.stop();
            // stop consumers before destroying or stopping their dependencies.
            // no more ready-audio blocks will be published.
            _audioSimulator.stop();
            // no more render tasks will be submitted.
            _executor.stop();

            _started = false;
    }

    bool Engine::post(Command command)
    {
        return _scheduler.post(command);
    }

    PlaybackSnapshot Engine::playbackSnapshot() const
    {
        return
        {
            _sharedState.playing.load(std::memory_order_acquire),
            _sharedState.generation.load(std::memory_order_acquire),
            _sharedState.audioCursorGeneration.load(std::memory_order_acquire),
            _sharedState.nextUnconsumedFrame.load(std::memory_order_acquire)
        };
    }

    EngineMetrics Engine::metrics() const
    {
        if (_started)
        throw std::logic_error("Engine metrics may be read only after stop()");

        return
        {
            _audioSimulator.getCallbacksCount(),
            _audioSimulator.getUnderrunsCount(),
            _audioSimulator.getCallbackMaxInUs()
        };
    }
    
    AudioBuffer Engine::capturedAudio() const
    {
        if (_started)
            throw std::logic_error("Read captured audio only after Engine::stop()");

        return _audioSimulator.capturedAudio();
    }

    void Engine::rethrowRenderFailure() const
    {
        _executor.rethrowFailure();
    }

    int Engine::calculateTotalFrames(const EngineSettings& settings)
    {
        if (settings.audio.sampleRate <= 0)
            throw std::invalid_argument("sampleRate must be greater than zero");

        if (settings.audio.channelCount <= 0)
            throw std::invalid_argument("channelCount must be greater than zero");

        if (settings.audio.audioBlockFrames <= 0 || settings.audio.audioBlockFrames > MAX_AUDIO_BLOCK_FRAMES)
        {
            throw std::invalid_argument("Invalid internal audio block size");
        }

        if (settings.timelineFrames < 0)
            throw std::invalid_argument("timelineFrames must not be negative");

        int totalFrames = settings.timelineFrames;

        if (totalFrames == 0)
        {
            if (settings.timelineInSeconds <= 0 || settings.timelineInSeconds > std::numeric_limits<int>::max() / settings.audio.sampleRate)
                throw std::invalid_argument("Invalid or overflowing timeline duration");

            totalFrames = settings.timelineInSeconds * settings.audio.sampleRate;
        }

        if (totalFrames % settings.audio.audioBlockFrames != 0 || settings.render.layout.chunkFrames() % settings.audio.audioBlockFrames != 0)
        {
            throw std::invalid_argument("Timeline and render chunks must contain complete audio blocks");
        }

        return totalFrames;
    }


} // namespace anasa