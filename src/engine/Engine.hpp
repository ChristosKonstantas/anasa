#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "engine/EngineSettings.hpp"
#include "utils/queues/SpscQueue.hpp"
#include "audio-pipeline/AudioConstants.hpp"
#include "audio-pipeline/AudioTypes.hpp"
#include "audio-pipeline/AudioSimulator.hpp"
#include "playback/PlaybackState.hpp"
#include "render/VersionTable.hpp"
#include "render/RenderConstants.hpp"
#include "render/Renderer.hpp"
#include "execution/Executor.hpp"

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
        SharedState                      _sharedState;
        SpscQueue<AudioBlock>            _readyAudioQueue;

        Renderer                         _renderer;
        Executor                         _executor;
        AudioSimulator                   _audioSimulator;

        bool                             _started;
    };
}

#endif // ENGINE_HPP