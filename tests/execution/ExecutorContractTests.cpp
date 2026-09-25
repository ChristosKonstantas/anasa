#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include "TestTileRenderer.hpp"
#include "functions/Functions.hpp"

namespace anasa
{

    TEST_CASE("Executor contract: injected renderer controls output cancellation and failure")
    {
        RenderOutcome outcome = RenderOutcome::Complete;

        TestTileRenderer renderer(outcome);
        ExecutorSettings settings;
        settings.workerCount = 2;
        settings.renderTasksQueueCapacity = TILES_PER_CHUNK;
        Executor executor(settings, renderer);

        std::shared_ptr<RenderJob> job = std::make_shared<RenderJob>();
        job->chunk = 0;
        job->version = 1;
        job->samples.fill(functions::UNTOUCHED_SAMPLE);

        executor.start();
        for (int tileIndex = 0; tileIndex < TILES_PER_CHUNK; ++tileIndex)
            REQUIRE(executor.submit({job, tileIndex})); // submit RenderTask

        std::shared_ptr<RenderJob> completed;
        REQUIRE(functions::waitForCompletedJob(executor, completed));
        executor.stop();

        REQUIRE(completed == job);
        REQUIRE(job->tilesRemaining.load(std::memory_order_acquire) == 0);
        REQUIRE(job->cancelled.load(std::memory_order_relaxed) == (outcome != RenderOutcome::Complete));

        for (int tile = 0; tile < TILES_PER_CHUNK; ++tile)
        {
            CAPTURE(tile);
            REQUIRE(renderer.calls[tile].load(std::memory_order_relaxed) == 1);
            const float expected = outcome == RenderOutcome::Complete ? TestTileRenderer::sampleForTile(tile) : functions::UNTOUCHED_SAMPLE;
            for (int frame = tile * TILE_FRAMES; frame < (tile + 1) * TILE_FRAMES; ++frame)
                REQUIRE(job->samples[frame] == expected);
        }

        std::shared_ptr<RenderJob> duplicate;
        REQUIRE_FALSE(executor.popCompleted(duplicate)); // no duplicate of job exists -> only one job in _completedJobsQueue 
    }
}