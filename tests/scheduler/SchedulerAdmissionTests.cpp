#include <memory>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "scheduler/SchedulerAdmissionTestRig.hpp"
#include "scheduler/SchedulerTestRig.hpp"

namespace anasa
{
    TEST_CASE("Scheduler: urgent admission survives repeated seeks with a full pending queue")
    {
        SchedulerSettings settings;
        settings.maxPendingTiles = GENERATE(2 * TILES_PER_CHUNK, 2 * TILES_PER_CHUNK + 1, 256);
        if (settings.maxPendingTiles < 256)
        {
            settings.urgentChunks = 1;
            settings.urgentReservedTiles = TILES_PER_CHUNK;
        }
        const bool visibleWork = GENERATE(false, true);
        CAPTURE(settings.maxPendingTiles, visibleWork);
        SchedulerAdmissionTestRig rig(settings);
        SchedulerTestRig::handleCommand(rig.scheduler, {CommandType::SetViewport, 0, visibleWork ? rig.TOTAL_FRAMES : 0});
        rig.executor.acceptsTasks = false;

        rig.requestPlayAt(64);
        rig.runIteration();
        rig.requestPlayAt(120);
        rig.runIteration();
        REQUIRE(SchedulerTestRig::pendingTileCount(rig.scheduler) + TILES_PER_CHUNK > settings.maxPendingTiles);

        rig.requestPlayAt(160);
        rig.executor.acceptsTasks = true;
        rig.runIteration();
        REQUIRE(rig.executor.queuedTaskCount() == 1);
        REQUIRE(rig.executor.nextJob()->chunk == 160);
        rig.requirePlaybackAt(160);
    }

    TEST_CASE("Scheduler: urgent admission accounts for partially submitted cancelled jobs")
    {
        SchedulerSettings settings;
        settings.urgentChunks = 1;
        settings.maxPendingTiles = TILES_PER_CHUNK;
        settings.urgentReservedTiles = TILES_PER_CHUNK;
        const bool workerFinishesFirst = GENERATE(false, true);
        CAPTURE(workerFinishesFirst);
        SchedulerAdmissionTestRig rig(settings);
        SchedulerTestRig::handleCommand(rig.scheduler, {CommandType::SetViewport, 0, 0});

        rig.requestPlayAt(64);
        rig.runIteration();
        std::shared_ptr<RenderJob> oldJob = rig.executor.nextJob();
        std::weak_ptr<RenderJob> oldLifetime = oldJob;
        if (workerFinishesFirst)
            rig.executor.completeNextTask();

        rig.requestPlayAt(120);
        rig.runIteration();
        REQUIRE(oldJob->cancelled.load());
        REQUIRE(oldJob->tilesRemaining.load() == (workerFinishesFirst ? 0 : 1));

        int targetChunk = 120;
        if (!workerFinishesFirst)
        {
            // Replace the old job with the same chunk/version before its completion arrives.
            targetChunk = 64;
            rig.requestPlayAt(targetChunk);
            rig.runIteration();
            rig.executor.completeNextTask();
            REQUIRE(oldJob->tilesRemaining.load() == 0);
            rig.runIteration();
            REQUIRE(rig.executor.nextJob() != oldJob);
            REQUIRE(rig.executor.nextJob()->chunk == targetChunk);
        }
        oldJob.reset();
        REQUIRE(oldLifetime.expired());
        rig.requirePlaybackAt(targetChunk);
    }

    TEST_CASE("Scheduler: urgent admission replaces cancelled work after an edit")
    {
        SchedulerSettings settings;
        settings.urgentChunks = 1;
        settings.maxPendingTiles = TILES_PER_CHUNK;
        settings.urgentReservedTiles = TILES_PER_CHUNK;
        SchedulerAdmissionTestRig rig(settings);
        SchedulerTestRig::handleCommand(rig.scheduler, {CommandType::SetViewport, 0, 0});
        rig.requestPlayAt(64);
        rig.runIteration();
        const std::shared_ptr<RenderJob> oldJob = rig.executor.nextJob();

        const int editedFrame = 64 * CHUNK_FRAMES + TILE_FRAMES;
        SchedulerTestRig::handleCommand(rig.scheduler, {CommandType::Edit, editedFrame, editedFrame});
        rig.runIteration();
        REQUIRE(oldJob->cancelled.load());
        REQUIRE(oldJob->tilesRemaining.load() == 1);
        REQUIRE(rig.versions.get(64) == oldJob->version + 1);

        rig.executor.completeNextTask();
        REQUIRE(oldJob->tilesRemaining.load() == 0);
        rig.runIteration();
        REQUIRE(rig.executor.nextJob() != oldJob);
        REQUIRE(rig.executor.nextJob()->version == rig.versions.get(64));
        rig.requirePlaybackAt(64);
    }

    TEST_CASE("Scheduler: urgent admission preserves FIFO work")
    {
        SchedulerSettings settings;
        settings.policyType = SchedulingPolicyType::Fifo;
        settings.urgentChunks = 1;
        settings.maxPendingTiles = TILES_PER_CHUNK;
        settings.urgentReservedTiles = TILES_PER_CHUNK;
        SchedulerAdmissionTestRig rig(settings);
        SchedulerTestRig::handleCommand(rig.scheduler, {CommandType::SetViewport, 0, 0});

        rig.requestPlayAt(64);
        rig.runIteration();
        const std::shared_ptr<RenderJob> firstJob = rig.executor.nextJob();
        rig.requestPlayAt(120);
        rig.runIteration();
        REQUIRE_FALSE(firstJob->cancelled.load());
        REQUIRE(SchedulerTestRig::pendingTileCount(rig.scheduler) == TILES_PER_CHUNK - 1);

        rig.requirePlaybackAt(120);
        REQUIRE_FALSE(firstJob->cancelled.load());
        REQUIRE(firstJob->tilesRemaining.load() == 0);
    }
} // namespace anasa