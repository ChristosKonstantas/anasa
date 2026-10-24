#ifndef SCHEDULER_ADMISSION_TEST_RIG_HPP
#define SCHEDULER_ADMISSION_TEST_RIG_HPP

#include <catch2/catch_test_macros.hpp>

#include "execution/ControlledRenderExecutor.hpp"
#include "render/Renderer.hpp"
#include "render/TestRenderKernel.hpp"
#include "render/VersionTable.hpp"
#include "scheduler/Scheduler.hpp"
#include "scheduler/SchedulerTestRig.hpp"

namespace anasa
{
    class SchedulerAdmissionTestRig
    {
    public:
        static constexpr int CHUNK_COUNT = 240;
        static constexpr int TOTAL_FRAMES = CHUNK_COUNT * CHUNK_FRAMES;

        explicit SchedulerAdmissionTestRig(const SchedulerSettings& settings)
            : maxPendingTiles(settings.maxPendingTiles),
              versions(CHUNK_COUNT),
              readyAudioQueue(READY_AUDIO_QUEUE_SLOTS, audioSettings.channelCount, audioSettings.audioBlockFrames),
              renderer(kernel, versions),
              executor(renderer),
              scheduler(settings, audioSettings, RenderSettings{}, TOTAL_FRAMES, state, versions, executor, readyAudioQueue)
        {}

        void requestPlayAt(int chunk)
        {
            SchedulerTestRig::handleCommand(scheduler, {CommandType::Seek, chunk * CHUNK_FRAMES, 0});
            SchedulerTestRig::handleCommand(scheduler, {CommandType::Play, 0, 0});
        }

        void runIteration()
        {
            SchedulerTestRig::runIteration(scheduler);
            REQUIRE(SchedulerTestRig::pendingTileCount(scheduler) <= maxPendingTiles);
        }

        void requirePlaybackAt(int chunk)
        {
            for (int iteration = 0; iteration <= 4 * TILES_PER_CHUNK && !state.playing.load(); ++iteration)
            {
                runIteration();
                if (executor.queuedTaskCount() > 0)
                    executor.completeNextTask();
            }
            REQUIRE(state.playing.load());
            const AudioBlock* block = readyAudioQueue.front();
            REQUIRE(block != nullptr);
            REQUIRE(block->generation == state.generation.load());
            REQUIRE(block->firstFrame == chunk * CHUNK_FRAMES);
            REQUIRE(block->frameCount == audioSettings.audioBlockFrames);
            for (int channel = 0; channel < audioSettings.channelCount; ++channel)
            {
                for (int frame = 0; frame < block->frameCount; ++frame)
                    REQUIRE(block->samples[channel][frame] == kernel.renderSample(channel, block->firstFrame + frame, versions.get(chunk)));
            }
        }

        const int                maxPendingTiles;
        AudioSettings            audioSettings;
        SharedState              state;
        VersionTable             versions;
        SpscQueue<AudioBlock>    readyAudioQueue;
        TestRenderKernel         kernel;
        Renderer                 renderer;
        ControlledRenderExecutor executor;
        Scheduler                scheduler;
    };
} // namespace anasa

#endif // SCHEDULER_ADMISSION_TEST_RIG_HPP