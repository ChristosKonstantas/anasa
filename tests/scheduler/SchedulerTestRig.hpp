#include "scheduler/Scheduler.hpp"

namespace anasa
{
    class SchedulerTestRig
    {
    public:
        static constexpr int CHUNK_COUNT = 16;
        static constexpr int TOTAL_FRAMES = CHUNK_COUNT * CHUNK_FRAMES;

        explicit SchedulerTestRig(SchedulingPolicyType policy = SchedulingPolicyType::Priority)
            :   audioSettings(makeAudioSettings()),
                renderSettings(makeRenderSettings()),
                schedulerSettings(makeSchedulerSettings(policy)),
                executorSettings(makeExecutorSettings()),
                sharedState(),
                versionTable(CHUNK_COUNT),
                readyAudioQueue(READY_AUDIO_QUEUE_SLOTS),
                renderer(audioSettings.sampleRate, renderSettings, versionTable),
                executor(executorSettings, renderer),
                scheduler(schedulerSettings, audioSettings, renderSettings, TOTAL_FRAMES, sharedState, 
                          versionTable, executor, readyAudioQueue)
        {
        }

        ~SchedulerTestRig()
        {
            scheduler.stop();
            executor.stop();
        }

        void start()
        {
            executor.start();
            scheduler.start();
        }

        void stop()
        {
            scheduler.stop();
            executor.stop();
        }

        AudioSettings audioSettings;
        RenderSettings renderSettings;
        SchedulerSettings schedulerSettings;
        ExecutorSettings executorSettings;

        SharedState sharedState;
        VersionTable versionTable;
        SpscQueue<AudioBlock> readyAudioQueue;

        Renderer renderer;
        Executor executor;
        Scheduler scheduler;

    private:
        static AudioSettings makeAudioSettings()
        {
            AudioSettings settings;

            settings.sampleRate = 48000;
            settings.audioBlockFrames = 128;
            settings.channelCount = 1;
            
            return settings;
        }

        static RenderSettings makeRenderSettings()
        {
            RenderSettings settings;

            settings.contextFrames = 0;
            settings.workIterations = 0;

            return settings;
        }

        static SchedulerSettings makeSchedulerSettings(SchedulingPolicyType policy)
        {
            SchedulerSettings settings;

            settings.policyType = policy;
            settings.commandQueueSlots = 32;
            settings.prebufferBlocks = 1;
            settings.lowWaterBlocks = 1;
            settings.highWaterBlocks = 4;
            settings.urgentChunks = 2;
            settings.maxPendingTiles = 64;
            settings.urgentReservedTiles = TILES_PER_CHUNK;
            settings.rebufferOnEdit = true;

            return settings;
        }

        static ExecutorSettings makeExecutorSettings()
        {
            ExecutorSettings settings;

            settings.workerCount = 2;
            settings.queuedTaskCapacity = 8;

            return settings;
        }
    };
}// namespace anasa