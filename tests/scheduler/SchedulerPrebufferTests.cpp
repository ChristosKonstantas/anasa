#include <algorithm>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "scheduler/SchedulerPrebufferTestRig.hpp"
#include "scheduler/SchedulerTestRig.hpp"

namespace anasa
{
    TEST_CASE("Scheduler: callbacks stop at the timeline end before the next scheduler iteration")
    {
        SchedulerSettings settings;
        settings.policyType = GENERATE(SchedulingPolicyType::Priority, SchedulingPolicyType::Fifo);
        SchedulerPrebufferTestRig rig(settings);
        rig.requestPlayAt(rig.TOTAL_FRAMES - rig.audioSettings.audioBlockFrames);
        SchedulerTestRig::runIteration(rig.scheduler);
        SchedulerTestRig::runIteration(rig.scheduler);
        REQUIRE(rig.state.playing.load());

        const AudioProcessResult last = rig.consumeBlock();
        REQUIRE(last.playing);
        REQUIRE_FALSE(last.underrun);
        REQUIRE(rig.state.nextUnconsumedFrame.load() == rig.TOTAL_FRAMES);

        // The scheduler has not run again, so playback permission is still true.
        for (int callback = 0; callback < 3; ++callback)
        {
            const AudioProcessResult result = rig.consumeBlock();
            REQUIRE_FALSE(result.playing);
            REQUIRE_FALSE(result.underrun);
            REQUIRE(rig.state.nextUnconsumedFrame.load() == rig.TOTAL_FRAMES);
        }
        REQUIRE(rig.state.playing.load());

        SchedulerTestRig::runIteration(rig.scheduler);
        REQUIRE_FALSE(rig.state.playing.load());
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