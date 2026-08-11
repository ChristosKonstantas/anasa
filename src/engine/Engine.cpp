#include "Engine.hpp"

namespace anasa
{

    Engine::Engine(EngineSettings settings)
        :_sharedState(),
         _readyAudioQueue(std::make_unique<ReadyAudioQueue>()),
         _audioSimulator(settings.audio, _sharedState, *_readyAudioQueue),
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