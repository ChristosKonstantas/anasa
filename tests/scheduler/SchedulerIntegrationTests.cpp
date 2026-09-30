#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>
#include <utility>
#include <catch2/generators/catch_generators.hpp>
#include <vector>

#include "audio/AudioBlockProcessor.hpp"
#include "utils/queues/SpscQueue.hpp"
#include "audio/AudioConstants.hpp"
#include "audio/AudioSettings.hpp"
#include "audio/AudioTypes.hpp"
#include "execution/Executor.hpp"
#include "execution/ExecutorSettings.hpp"
#include "playback/PlaybackState.hpp"
#include "render/RenderConstants.hpp"
#include "render/Renderer.hpp"
#include "render/RenderSettings.hpp"
#include "render/VersionTable.hpp"
#include "scheduler/Scheduler.hpp"
#include "scheduler/SchedulerSettings.hpp"
#include "scheduler/SchedulerTypes.hpp"
#include "functions/Functions.hpp"
#include "scheduler/SchedulerTestRig.hpp"

namespace anasa
{    

    using namespace std::chrono_literals;

    TEST_CASE("Scheduler: post rejects commands before start and after stop")
    {
        SchedulerTestRig schedulerTestRig;
        const Command pause{CommandType::Pause, 0, 0};

        REQUIRE_FALSE(schedulerTestRig.scheduler.post(pause));

        schedulerTestRig.start();

        REQUIRE(schedulerTestRig.scheduler.post(pause));

        schedulerTestRig.stop();

        REQUIRE_FALSE(schedulerTestRig.scheduler.post(pause));
    }

    TEST_CASE("Scheduler: publishes audio blocks in strict timeline order")
    {
        SchedulerTestRig schedulerTestRig;
        schedulerTestRig.start();

        constexpr int blocksToObserve = 8;

        for (int blockIndex = 0; blockIndex < blocksToObserve; ++blockIndex)
        {
            functions::BlockHeader block;

            REQUIRE(functions::waitAndPopBlock(schedulerTestRig.readyAudioQueue, block));

            REQUIRE(block.generation == 1);

            REQUIRE(block.firstFrame == blockIndex * schedulerTestRig.audioSettings.audioBlockFrames);

            REQUIRE(block.frameCount == schedulerTestRig.audioSettings.audioBlockFrames);
        }
    }

    TEST_CASE("Scheduler: Play prebuffers before enabling playback and Pause disables it")
    {
        SchedulerTestRig schedulerTestRig;
        schedulerTestRig.start();

        REQUIRE(schedulerTestRig.scheduler.post({CommandType::Play, 0, 0}));

        REQUIRE(functions::waitUntil([&]{return schedulerTestRig.sharedState.playing.load(std::memory_order_acquire);}));

        REQUIRE(schedulerTestRig.scheduler.post({CommandType::Pause, 0, 0}));

        REQUIRE(functions::waitUntil([&]{return !schedulerTestRig.sharedState.playing.load(std::memory_order_acquire);}));
    }

    TEST_CASE("Scheduler: Edit invalidates exactly the edited chunk when context is zero")
    {
        SchedulerTestRig schedulerTestRig;
        schedulerTestRig.start();

        constexpr int editedChunk = 6;
        constexpr int editedFrame = editedChunk * CHUNK_FRAMES + 100;

        const int initialGeneration = schedulerTestRig.sharedState.generation.load(std::memory_order_acquire);

        REQUIRE(schedulerTestRig.versionTable.get(editedChunk - 1) == 1);

        REQUIRE(schedulerTestRig.versionTable.get(editedChunk) == 1);

        REQUIRE(schedulerTestRig.versionTable.get(editedChunk + 1) == 1);

        REQUIRE(schedulerTestRig.scheduler.post({CommandType::Edit, editedFrame, editedFrame}));

        REQUIRE(functions::waitUntil([&]
        {
            return schedulerTestRig.versionTable.get(editedChunk) == 2 &&
                schedulerTestRig.sharedState.generation.load(std::memory_order_acquire) == initialGeneration + 1;
        }));

        REQUIRE(schedulerTestRig.versionTable.get(editedChunk - 1) == 1);

        REQUIRE(schedulerTestRig.versionTable.get(editedChunk + 1) == 1);

        REQUIRE(schedulerTestRig.sharedState.targetFrame.load(std::memory_order_acquire) == 0);
    }

    TEST_CASE("Scheduler: stale old-generation cursor cannot overwrite a backward seek")
    {
        SchedulerTestRig schedulerTestRig;

        constexpr int oldCursor = 8 * CHUNK_FRAMES;

        schedulerTestRig.sharedState.nextUnconsumedFrame.store(oldCursor, std::memory_order_relaxed);

        schedulerTestRig.sharedState.audioCursorGeneration.store(1, std::memory_order_relaxed);

        schedulerTestRig.start();

        // Keep the producer blocked so no new-generation block can be published before the stale cursor is injected.
        REQUIRE(functions::waitUntil([&] {return schedulerTestRig.readyAudioQueue.isFull();}, 5000ms));

        constexpr int requestedTarget = CHUNK_FRAMES + 37;

        constexpr int expectedTarget = CHUNK_FRAMES;

        const int oldGeneration = schedulerTestRig.sharedState.generation.load(std::memory_order_acquire);

        REQUIRE(schedulerTestRig.scheduler.post({CommandType::Seek, requestedTarget, 0}));

        REQUIRE(functions::waitUntil([&]
        {
            return schedulerTestRig.sharedState.generation.load(std::memory_order_acquire) == oldGeneration + 1 &&
                   schedulerTestRig.sharedState.targetFrame.load(std::memory_order_acquire) == expectedTarget;
        }));

        // Simulate an in-flight callback from the previous generation publishing progress after the seek.
        // Its cursor-generation tag intentionally remains old.
        schedulerTestRig.sharedState.nextUnconsumedFrame.store(oldCursor + schedulerTestRig.audioSettings.audioBlockFrames,  std::memory_order_release);

        functions::BlockHeader firstNewBlock;

        REQUIRE(functions::waitAndPopFirstBlockOfGeneration(schedulerTestRig.readyAudioQueue, oldGeneration + 1, firstNewBlock));

        REQUIRE(firstNewBlock.firstFrame == expectedTarget);

        REQUIRE(firstNewBlock.frameCount == schedulerTestRig.audioSettings.audioBlockFrames);
    }

    TEST_CASE("Scheduler: reaching the timeline end disables playback")
    {
        SchedulerTestRig schedulerTestRig;
        schedulerTestRig.start();
        REQUIRE(schedulerTestRig.scheduler.post({CommandType::Play, 0, 0}));
        REQUIRE(functions::waitUntil([&]{return schedulerTestRig.sharedState.playing.load(std::memory_order_acquire);}));

        // Model the consumer reaching the end in the current generation.
        schedulerTestRig.sharedState.nextUnconsumedFrame.store(SchedulerTestRig::TOTAL_FRAMES, std::memory_order_release);
        REQUIRE(functions::waitUntil([&]{return !schedulerTestRig.sharedState.playing.load(std::memory_order_acquire);}));
    }

    TEST_CASE("Scheduler: preserves channel samples through chunk boundaries seek and edit")
    {
        const int channelCount = GENERATE(1, 2, 6, 16);
        const SchedulingPolicyType policy = GENERATE(SchedulingPolicyType::Priority, SchedulingPolicyType::Fifo);

        SchedulerTestRig rig(policy, channelCount);
        const int blockFrames = rig.audioSettings.audioBlockFrames;
        AudioBlockProcessor processor(blockFrames, rig.sharedState, rig.readyAudioQueue);
        AudioBuffer output(channelCount, blockFrames);
        std::vector<float*> channels(channelCount);

        for (int channel = 0; channel < channelCount; ++channel)
            channels[channel] = output[channel].data();

        const auto consumeBlock = [&](int firstFrame, int version, int generation)
        {
            REQUIRE(functions::waitUntil([&]
            {
                return rig.readyAudioQueue.front() != nullptr;
            }));

            const AudioBlock* block = rig.readyAudioQueue.front();

            REQUIRE(block->firstFrame == firstFrame);
            REQUIRE(block->generation == generation);
            REQUIRE(block->frameCount == blockFrames);
            REQUIRE(block->samples.channelCount() == channelCount);

            const AudioProcessResult result = processor.processBlock({channels, blockFrames});

            REQUIRE(result.playing);
            REQUIRE_FALSE(result.underrun);
            REQUIRE(rig.sharedState.nextUnconsumedFrame.load() == firstFrame + blockFrames);

            for (int channel = 0; channel < channelCount; ++channel)
            {
                for (int frame = 0; frame < blockFrames; ++frame)
                {
                    CAPTURE(channel, frame, firstFrame, version, generation);

                    REQUIRE(output[channel][frame] == rig.renderKernel.renderSample(channel, firstFrame + frame, version));
                }
            }
        };

        const auto play = [&]
        {
            REQUIRE(rig.scheduler.post({CommandType::Play, 0, 0}));

            REQUIRE(functions::waitUntil([&]
            {
                return rig.sharedState.playing.load(std::memory_order_acquire);
            }));
        };

        const auto pause = [&]
        {
            REQUIRE(rig.scheduler.post({CommandType::Pause, 0, 0}));

            REQUIRE(functions::waitUntil([&]
            {
                return !rig.sharedState.playing.load(std::memory_order_acquire);
            }));
        };

        const auto acknowledgeGeneration = [&](int generation, int target)
        {
            REQUIRE(functions::waitUntil([&]
            {
                return rig.sharedState.generation.load(std::memory_order_acquire) == generation;
            }));

            // A paused callback acknowledges the cursor and discards stale audio.
            REQUIRE_FALSE(processor.processBlock({channels, blockFrames}).playing);
            REQUIRE(rig.sharedState.audioCursorGeneration.load() == generation);
            REQUIRE(rig.sharedState.nextUnconsumedFrame.load() == target);

            for (int channel = 0; channel < channelCount; ++channel)
                for (float sample : output[channel])
                    REQUIRE(sample == 0.0f);
        };

        rig.start();
        play();

        // Include the first block from the following chunk.
        for (int block = 0; block <= CHUNK_FRAMES / blockFrames; ++block)
            consumeBlock(block * blockFrames, 1, 1);

        REQUIRE(functions::waitUntil([&]{return rig.readyAudioQueue.isFull();}));

        pause();

        const int seekTarget = CHUNK_FRAMES + 2 * blockFrames;
        REQUIRE(rig.scheduler.post({CommandType::Seek, seekTarget + 7, 0}));

        acknowledgeGeneration(2, seekTarget);
        play();
        consumeBlock(seekTarget, 1, 2);

        pause();

        const int editTarget = seekTarget + blockFrames;
        REQUIRE(rig.scheduler.post({CommandType::Edit, editTarget, editTarget}));

        acknowledgeGeneration(3, editTarget);
        REQUIRE(rig.versionTable.get(editTarget / CHUNK_FRAMES) == 2);

        play();
        consumeBlock(editTarget, 2, 3);
    }
} // namespace anasa