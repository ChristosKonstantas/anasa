#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <functional>
#include <memory>

#include "engine/EngineSettings.hpp"
#include "engine/EngineTypes.hpp"
#include "utils/queues/SpscQueue.hpp"
#include "audio/AudioConstants.hpp"
#include "audio/AudioTypes.hpp"
#include "audio/simulator/AudioSimulator.hpp"
#include "audio/AudioBlockProcessor.hpp"
#include "playback/PlaybackState.hpp"
#include "render/VersionTable.hpp"
#include "render/RenderConstants.hpp"
#include "render/IChunkVersionReader.hpp"
#include "render/ITileRenderer.hpp"
#include "execution/Executor.hpp"
#include "scheduler/Scheduler.hpp"
#include "scheduler/SchedulerTypes.hpp"

namespace anasa
{
    using RendererFactory = std::function<std::unique_ptr<ITileRenderer>(const AudioSettings&, const RenderSettings&, const IChunkVersionReader&)>;

    class Engine
    {
    public:
        // An empty factory selects the synthetic renderer. A supplied factory is called once
        // during construction and must return a non-null renderer owned by this engine.
        // The version reader remains valid until the renderer is destroyed.
        explicit Engine(EngineSettings settings = {}, RendererFactory rendererFactory = {});
        ~Engine();

        void start();
        void stop();
        bool post(Command command);
        PlaybackSnapshot playbackSnapshot() const;
        EngineMetrics metrics() const;

    private:
        static int calculateTotalFrames(const EngineSettings &settings);
        static std::unique_ptr<ITileRenderer> createRenderer(const RendererFactory& factory,
            const AudioSettings& audio, const RenderSettings& render, const IChunkVersionReader& versions);

        EngineSettings                   _settings;
        int                              _totalFrames;
        int                              _chunkCount;

        VersionTable                     _versionTable;
        SharedState                      _sharedState;
        SpscQueue<AudioBlock>             _readyAudioQueue;

        std::unique_ptr<ITileRenderer>    _renderer;
        Executor                         _executor;
        Scheduler                        _scheduler;
        AudioBlockProcessor              _audioBlockProcessor;
        AudioSimulator                   _audioSimulator;

        bool                             _started;
    };
}

#endif // ENGINE_HPP