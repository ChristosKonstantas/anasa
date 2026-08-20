#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "engine/EngineSettings.hpp"
#include "utils/queues/SpscQueue.hpp"
#include "audio-pipeline/AudioSimulator.hpp"
#include "playback/PlaybackState.hpp"
#include "render/VersionTable.hpp"
#include "render/RenderConstants.hpp"

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
        EngineSettings                   _settings;
        int                              _totalFrames;
        int                              _chunkCount;
        VersionTable                     _versionTable;

        // --- (1) Simulated real-time audio thread.  ---// 
        SharedState                      _sharedState;
        SpscQueue<AudioBlock>            _readyAudioQueue;
        AudioSimulator                   _audioSimulator;
        bool                             _started;
    };
}

#endif // ENGINE_HPP