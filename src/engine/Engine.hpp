#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "EngineTypes.hpp"
#include "EngineSettings.hpp"
#include "audio-pipeline/AudioSimulator.hpp"

namespace anasa
{
    class Engine
    {
    public:
        Engine();
        ~Engine();

        void start();
        void stop();

    private:

        // (1) Simulated real-time audio thread.
        SharedState        _sharedState;

        ReadyAudioQueue    _readyAudioQueue;
        AudioSimulator     _audioSimulator;
        bool               _started;
    };
}

#endif ENGINE_HPP