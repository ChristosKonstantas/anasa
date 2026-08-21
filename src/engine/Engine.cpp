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

        _audioSimulator.start();

        _started = true;
    }

    void Engine::stop()
    {
        if (!_started)
            return;

        _sharedState.stop.store(true, std::memory_order_release);

        _audioSimulator.stop();

        _started = false;
    }

} // namespace anasa