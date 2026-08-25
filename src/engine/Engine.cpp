#include "Engine.hpp"

namespace anasa
{

    Engine::Engine(EngineSettings settings)
        :_settings(settings),
         _totalFrames(_settings.timelineInSeconds * _settings.audio.sampleRate),
         _chunkCount((_totalFrames + CHUNK_FRAMES - 1) / CHUNK_FRAMES), // (ceil(_totalFrames/CHUNK_FRAMES))
         _versionTable(_chunkCount),
         _sharedState(),
         _readyAudioQueue(READY_AUDIO_QUEUE_SLOTS),
         _renderer(_settings.audio.sampleRate, _settings.render, _versionTable),
         _executor(_settings.executor, _renderer),
         _audioSimulator(_settings.audio, _sharedState, _readyAudioQueue),
         _started(false)
    {
    }

    Engine::~Engine()
    {
        stop();
    }
    
    void Engine::start()
    {
        if (_started)
            return;

        _sharedState.stop.store(false, std::memory_order_release);

        try
        {
            // start dependencies before their future producer/consumer pipeline.
            _executor.start();
            _audioSimulator.start();
        }
        catch (...)
        {
            // clean up if either component fails to start..
            _sharedState.stop.store(true, std::memory_order_release);

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

            // stop consumers before destroying or stopping their dependencies.
            _audioSimulator.stop();
            _executor.stop();

            _started = false;
    }

} // namespace anasa