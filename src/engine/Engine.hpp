#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <memory>

#include "engine/EngineTypes.hpp"
#include "engine/EngineSettings.hpp"
#include "utils/queues/SpscQueue.hpp"
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
        SpscQueue<AudioBlock>            _readyAudioQueue;
        AudioSimulator                   _audioSimulator;
        bool                             _started;
    };
}

#endif // ENGINE_HPP