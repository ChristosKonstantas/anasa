#include <algorithm>
#include <atomic>
#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "audio/AudioBlockProcessor.hpp"
#include "execution/IRenderExecutor.hpp"
#include "execution/RenderTaskProcessor.hpp"
#include "render/Renderer.hpp"
#include "render/TestRenderKernel.hpp"
#include "render/VersionTable.hpp"
#include "scheduler/Scheduler.hpp"
#include "scheduler/SchedulerTestRig.hpp"

namespace anasa
{
    namespace
    {
        // Complete accepted work inline so scheduling tests do not depend on timing.
        class ImmediateRenderExecutor final : public IRenderExecutor
        {
        public:
            explicit ImmediateRenderExecutor(const ITileRenderer& renderer)
                : _processor(renderer)
            {}

            bool submit(RenderTask task) override
            {
                if (_processor.process(task, _stopRequested))
                    _completedJobs.push(std::move(task.job));
                return true;
            }

            bool popCompleted(std::shared_ptr<RenderJob>& job) override
            {
                if (_completedJobs.empty())
                    return false;
                job = std::move(_completedJobs.front());
                _completedJobs.pop();
                return true;
            }

            int workerCount() const override { return 1; }
            int queuedTaskCount() override { return 0; }

        private:
            RenderTaskProcessor                    _processor;
            std::atomic<bool>                      _stopRequested{false};
            std::queue<std::shared_ptr<RenderJob>> _completedJobs;
        };

        class SchedulerPrebufferTestRig
        {
        public:
            static constexpr int CHUNK_COUNT = 16;
            static constexpr int TOTAL_FRAMES = CHUNK_COUNT * CHUNK_FRAMES;

            explicit SchedulerPrebufferTestRig(const SchedulerSettings& settings)
                : versions(CHUNK_COUNT),
                  readyAudioQueue(READY_AUDIO_QUEUE_SLOTS, audioSettings.channelCount, audioSettings.audioBlockFrames),
                  renderer(kernel, versions),
                  executor(renderer),
                  scheduler(settings, audioSettings, RenderSettings{}, TOTAL_FRAMES, state, versions, executor, readyAudioQueue),
                  processor(audioSettings.audioBlockFrames, state, readyAudioQueue),
                  output(audioSettings.channelCount, audioSettings.audioBlockFrames),
                  outputChannels(audioSettings.channelCount)
            {
                for (int channel = 0; channel < audioSettings.channelCount; ++channel)
                    outputChannels[channel] = output[channel].data();
            }

            void requestPlayAt(int frame)
            {
                SchedulerTestRig::handleCommand(scheduler, {CommandType::SetViewport, 0, 0});
                SchedulerTestRig::handleCommand(scheduler, {CommandType::Seek, frame, 0});
                SchedulerTestRig::handleCommand(scheduler, {CommandType::Play, 0, 0});
            }

            AudioProcessResult consumeBlock()
            {
                return processor.processBlock({outputChannels, audioSettings.audioBlockFrames});
            }

            AudioSettings           audioSettings;
            SharedState             state;
            VersionTable            versions;
            SpscQueue<AudioBlock>   readyAudioQueue;
            TestRenderKernel        kernel;
            Renderer                renderer;
            ImmediateRenderExecutor executor;
            Scheduler               scheduler;
            AudioBlockProcessor     processor;
            AudioBuffer             output;
            std::vector<float*>     outputChannels;
        };
    }

    TEST_CASE("Scheduler: prebuffer demand remains urgent outside the viewport")
    {
        SchedulerSettings settings;
        settings.urgentChunks = 1;
        settings.prebufferBlocks = GENERATE(0, 1, 16, 17, 32, 64);
        settings.policyType = GENERATE(SchedulingPolicyType::Priority, SchedulingPolicyType::Fifo);
        SchedulerPrebufferTestRig rig(settings);
        const int blockFrames = rig.audioSettings.audioBlockFrames;
        const int target = GENERATE(0, 128, CHUNK_FRAMES - 128, 6 * CHUNK_FRAMES + 128,
                                   SchedulerPrebufferTestRig::TOTAL_FRAMES - 2 * CHUNK_FRAMES,
                                   SchedulerPrebufferTestRig::TOTAL_FRAMES - 128);
        CAPTURE(settings.prebufferBlocks, settings.policyType, target);
        rig.requestPlayAt(target);

        SchedulerTestRig::runIteration(rig.scheduler);
        if (settings.prebufferBlocks > 0)
            REQUIRE_FALSE(rig.state.playing.load());
        SchedulerTestRig::runIteration(rig.scheduler);
        REQUIRE(rig.state.playing.load());

        const int remainingBlocks = (rig.TOTAL_FRAMES - target) / blockFrames;
        const int blocksToConsume = std::max(1, std::min(settings.prebufferBlocks, remainingBlocks));
        for (int block = 0; block < blocksToConsume; ++block)
        {
            const AudioProcessResult result = rig.consumeBlock();
            REQUIRE(result.playing);
            REQUIRE_FALSE(result.underrun);
        }
        REQUIRE(rig.state.nextUnconsumedFrame.load() == target + blocksToConsume * blockFrames);
    }

    TEST_CASE("Scheduler: prebuffer demand refreshes pending work within a chunk")
    {
        SchedulerSettings settings;
        settings.urgentChunks = 1;
        settings.prebufferBlocks = 16;
        SchedulerPrebufferTestRig rig(settings);
        rig.requestPlayAt(0);
        SchedulerTestRig::runIteration(rig.scheduler);
        SchedulerTestRig::runIteration(rig.scheduler);
        REQUIRE(rig.state.playing.load());

        const AudioProcessResult first = rig.consumeBlock();
        REQUIRE(first.playing);
        REQUIRE_FALSE(first.underrun);
        REQUIRE(rig.state.nextUnconsumedFrame.load() == rig.audioSettings.audioBlockFrames);

        // Chunk 1 is already pending as background work. Moving one block makes it urgent.
        SchedulerTestRig::runIteration(rig.scheduler);
        SchedulerTestRig::runIteration(rig.scheduler);
        for (int block = 0; block < settings.prebufferBlocks; ++block)
        {
            const AudioProcessResult result = rig.consumeBlock();
            REQUIRE(result.playing);
            REQUIRE_FALSE(result.underrun);
        }
        REQUIRE(rig.state.nextUnconsumedFrame.load() == CHUNK_FRAMES + rig.audioSettings.audioBlockFrames);
    }

    TEST_CASE("Scheduler: prebuffer demand preserves the configured urgent window")
    {
        SchedulerSettings settings;
        settings.urgentChunks = 3;
        settings.prebufferBlocks = 1;
        SchedulerPrebufferTestRig rig(settings);
        rig.requestPlayAt(0);
        SchedulerTestRig::runIteration(rig.scheduler);
        SchedulerTestRig::runIteration(rig.scheduler);
        REQUIRE(rig.state.playing.load());

        const int blocksToConsume = settings.urgentChunks * CHUNK_FRAMES / rig.audioSettings.audioBlockFrames;
        for (int block = 0; block < blocksToConsume; ++block)
        {
            const AudioProcessResult result = rig.consumeBlock();
            REQUIRE(result.playing);
            REQUIRE_FALSE(result.underrun);
        }
    }

    TEST_CASE("Scheduler: prebuffering progresses with one pending chunk of capacity")
    {
        SchedulerSettings settings;
        settings.urgentChunks = 1;
        settings.prebufferBlocks = READY_AUDIO_QUEUE_SLOTS;
        settings.maxPendingTiles = TILES_PER_CHUNK;
        settings.urgentReservedTiles = TILES_PER_CHUNK;
        SchedulerPrebufferTestRig rig(settings);
        rig.requestPlayAt(rig.audioSettings.audioBlockFrames);

        // Even with serial completion, one iteration per chunk plus collection is sufficient.
        for (int iteration = 0; iteration <= rig.CHUNK_COUNT && !rig.state.playing.load(); ++iteration)
            SchedulerTestRig::runIteration(rig.scheduler);
        REQUIRE(rig.state.playing.load());

        for (int block = 0; block < settings.prebufferBlocks; ++block)
        {
            const AudioProcessResult result = rig.consumeBlock();
            REQUIRE(result.playing);
            REQUIRE_FALSE(result.underrun);
        }
    }
}