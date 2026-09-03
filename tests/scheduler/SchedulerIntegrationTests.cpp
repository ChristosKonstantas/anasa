#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>
#include <utility>

#include "utils/queues/SpscQueue.hpp"
#include "audio-pipeline/AudioConstants.hpp"
#include "audio-pipeline/AudioSettings.hpp"
#include "audio-pipeline/AudioTypes.hpp"
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

} // namespace anasa