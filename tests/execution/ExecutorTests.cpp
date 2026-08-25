#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include "execution/Executor.hpp"
#include "render/Renderer.hpp"
#include "render/VersionTable.hpp"

#include "functions/Functions.hpp"

namespace anasa
{
    TEST_CASE("Executor: rejects invalid settings")
    {
        VersionTable versions(4);
        RenderSettings renderSettings = functions::makeTestRenderSettings();
        Renderer renderer(48000, renderSettings, versions);

        SECTION("worker count must be positive")
        {
            ExecutorSettings settings = functions::makeTestExecutorSettings();
            settings.workerCount = 0;

            REQUIRE_THROWS_AS(Executor(settings, renderer), std::invalid_argument);
        }

        SECTION("queue capacity must be positive")
        {
            ExecutorSettings settings = functions::makeTestExecutorSettings();
            settings.queuedTaskCapacity = 0;

            REQUIRE_THROWS_AS(Executor(settings, renderer), std::invalid_argument);
        }
    }

    TEST_CASE("Executor: start and stop are idempotent")
    {
        VersionTable versions(4);
        RenderSettings renderSettings = functions::makeTestRenderSettings();
        Renderer renderer(48000, renderSettings, versions);
        ExecutorSettings executorSettings = functions::makeTestExecutorSettings();
        Executor executor(executorSettings, renderer);

        REQUIRE_NOTHROW(executor.start());
        REQUIRE_NOTHROW(executor.start());

        REQUIRE(executor.workerCount() == executorSettings.workerCount);

        REQUIRE_NOTHROW(executor.stop());
        REQUIRE_NOTHROW(executor.stop());

        // Starting again after a complete stop is supported.
        REQUIRE_NOTHROW(executor.start());
        REQUIRE_NOTHROW(executor.stop());
    }

    TEST_CASE("Executor: rejects submissions while stopped")
    {
        VersionTable versions(4);
        RenderSettings renderSettings = functions::makeTestRenderSettings();
        Renderer renderer(48000, renderSettings, versions);
        ExecutorSettings executorSettings = functions::makeTestExecutorSettings();
        Executor executor(executorSettings, renderer);

        std::shared_ptr<RenderJob> job = functions::makeTestRenderJob(versions, 1);

        REQUIRE_FALSE(executor.submit({job, 0}));

        executor.start();
        executor.stop();

        REQUIRE_FALSE(executor.submit({job, 0}));
    }

    TEST_CASE("Executor: rejects invalid render tasks")
    {
        VersionTable versions(4);
        RenderSettings renderSettings = functions::makeTestRenderSettings();
        Renderer renderer(48000, renderSettings, versions);
        ExecutorSettings executorSettings = functions::makeTestExecutorSettings();
        Executor executor(executorSettings, renderer);

        std::shared_ptr<RenderJob> job = functions::makeTestRenderJob(versions, 1);

        REQUIRE_THROWS_AS(executor.submit({nullptr, 0}), std::invalid_argument);
        REQUIRE_THROWS_AS(executor.submit({job, -1}), std::out_of_range);
        REQUIRE_THROWS_AS(executor.submit({job, TILES_PER_CHUNK}), std::out_of_range);
    }

    TEST_CASE("Executor: workers render every tile and publish one completed job")
    {
        VersionTable versions(4);
        RenderSettings renderSettings = functions::makeTestRenderSettings();
        Renderer renderer(48000, renderSettings, versions);
        ExecutorSettings executorSettings = functions::makeTestExecutorSettings();
        Executor executor(executorSettings, renderer);

        std::shared_ptr<RenderJob> job = functions::makeTestRenderJob(versions, 1);
        std::shared_ptr<RenderJob> completedJob;

        executor.start();
        for (int tileIndex = 0; tileIndex < TILES_PER_CHUNK; ++tileIndex)
        {
            CAPTURE(tileIndex);
            REQUIRE(executor.submit({job, tileIndex}));
        }

        REQUIRE(functions::waitForCompletedJob(executor, completedJob));

        executor.stop();

        REQUIRE(completedJob == job);
        REQUIRE(job->tilesRemaining.load(std::memory_order_acquire) == 0);
        REQUIRE_FALSE(job->cancelled.load(std::memory_order_acquire));

        for (int frame = 0; frame < CHUNK_FRAMES; ++frame)
        {
            CAPTURE(frame);

            REQUIRE(job->samples[frame] != functions::UNTOUCHED_SAMPLE);
            REQUIRE(std::isfinite(job->samples[frame]));
            REQUIRE(std::abs(job->samples[frame]) <= 1.0f);
        }

        // A job must be published only once, by its final tile.
        std::shared_ptr<RenderJob> duplicateCompletion;
        REQUIRE_FALSE(executor.popCompleted(duplicateCompletion));
    }

    TEST_CASE("Executor: obsolete job completes as cancelled")
    {
        VersionTable versions(4);
        RenderSettings renderSettings = functions::makeTestRenderSettings();
        Renderer renderer(48000, renderSettings, versions);
        ExecutorSettings executorSettings = functions::makeTestExecutorSettings();
        Executor executor(executorSettings, renderer);

        std::shared_ptr<RenderJob> job = functions::makeTestRenderJob(versions, 1);
        std::shared_ptr<RenderJob> completedJob;

        // The job contains the old version after this edit.
        versions.bump(job->chunk);

        REQUIRE(versions.get(job->chunk) != job->version);

        executor.start();
        
        for (int tileIndex = 0; tileIndex < TILES_PER_CHUNK; ++tileIndex)
        {
            CAPTURE(tileIndex);
            REQUIRE(executor.submit({job, tileIndex}));
        }

        REQUIRE(functions::waitForCompletedJob(executor, completedJob));

        executor.stop();

        REQUIRE(completedJob == job);
        REQUIRE(job->tilesRemaining.load(std::memory_order_acquire) == 0);
        REQUIRE(job->cancelled.load(std::memory_order_acquire));

        // Cancellation happened before rendering started.
        for (int frame = 0; frame < CHUNK_FRAMES; ++frame)
        {
            CAPTURE(frame);
            REQUIRE(job->samples[frame] == functions::UNTOUCHED_SAMPLE);
        }
    }

    TEST_CASE("Executor: explicitly cancelled job does not render")
    {
        VersionTable versions(4);
        RenderSettings renderSettings = functions::makeTestRenderSettings();
        Renderer renderer(48000, renderSettings, versions);
        ExecutorSettings executorSettings = functions::makeTestExecutorSettings();
        Executor executor(executorSettings, renderer);

        std::shared_ptr<RenderJob> job = functions::makeTestRenderJob(versions, 1);
        std::shared_ptr<RenderJob> completedJob;

        job->cancelled.store(true, std::memory_order_release);

        executor.start();

        for (int tileIndex = 0; tileIndex < TILES_PER_CHUNK; ++tileIndex)
        {
            CAPTURE(tileIndex);
            REQUIRE(executor.submit({job, tileIndex}));
        }

        REQUIRE(functions::waitForCompletedJob(executor, completedJob));

        executor.stop();

        REQUIRE(completedJob == job);
        REQUIRE(job->tilesRemaining.load(std::memory_order_acquire) == 0);
        REQUIRE(job->cancelled.load(std::memory_order_acquire));

        for (int frame = 0; frame < CHUNK_FRAMES; ++frame)
        {
            CAPTURE(frame);
            REQUIRE(job->samples[frame] == functions::UNTOUCHED_SAMPLE);
        }
    }

} // namespace anasa