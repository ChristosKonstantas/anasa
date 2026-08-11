#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <memory>

#include "engine/EngineTypes.hpp"
#include "engine/EngineSettings.hpp"
#include "audio-pipeline/AudioQueue.hpp"
#include "audio-pipeline/AudioSimulator.hpp"
#include "playback/PlaybackState.hpp"

namespace anasa
{
    class Engine
    {
    public:
        explicit Engine(EngineSettings settings = {});
        ~Engine();

        void start();
        void stop();

    private:

        // (1) Simulated real-time audio thread.
        SharedState                      _sharedState;
        std::unique_ptr<ReadyAudioQueue> _readyAudioQueue;
        AudioSimulator                   _audioSimulator;
        bool                             _started;
    };
}

#endif ENGINE_HPP