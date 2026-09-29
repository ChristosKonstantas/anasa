#include <array>
#include <atomic>
#include <cmath>
#include <stdexcept>
#include <thread>
#include <iostream>

#include <catch2/catch_test_macros.hpp>

#include "render/Renderer.hpp"
#include "render/RenderConstants.hpp"
#include "render/RenderSettings.hpp"
#include "render/RenderTypes.hpp"
#include "render/VersionTable.hpp"
#include "render/kernels/SyntheticRenderKernel.hpp"

#ifdef enable_benchmarks
#include "benchmarks/Benchmark.hpp"
#endif

#include "functions/Functions.hpp"
#include "TestVersionReader.hpp"
#include "TestRenderKernel.hpp"

namespace anasa
{
    /********************** VERSION TABLE TESTS **********************/
    TEST_CASE("VersionTable: initializes every chunk with version one")
    {
        VersionTable versions(4);
        const IChunkVersionReader& reader = versions;

        REQUIRE(reader.count() == 4);

        for (int chunk = 0; chunk < reader.count(); ++chunk)
            REQUIRE(reader.get(chunk) == 1);
    }

    TEST_CASE("VersionTable: bump changes only the selected chunk")
    {
        VersionTable versions(4);

        int newVersion = versions.bump(2);

        REQUIRE(newVersion == 2);
        REQUIRE(versions.get(2) == 2);
        REQUIRE(versions.get(0) == 1);
        REQUIRE(versions.get(1) == 1);
        REQUIRE(versions.get(3) == 1);
    }

    TEST_CASE("VersionTable: concurrent bumps are not lost")
    {
        constexpr int THREAD_COUNT = 4;
        constexpr int BUMPS_PER_THREAD = 10000;

        VersionTable versions(1);
        std::array<std::thread, THREAD_COUNT> threads;
        
        // Counts how many workers have reached the starting gate.
        std::atomic<int> readyThreads{0};

        // All workers wait while this remains false.
        std::atomic<bool> start{false};
        
        // This test forces all four threads to reach a common starting point 
        // before any of them begins incrementing the version.
        for (int thread = 0; thread < THREAD_COUNT; ++thread)
        {
            // Constructing std::thread starts this lambda immediately.
            threads[thread] = std::thread([&versions, &readyThreads, &start]
            {
                // This worker announces that it has reached the gate.
                readyThreads.fetch_add(1, std::memory_order_release);

                // Wait until the main thread opens the gate.
                while (!start.load(std::memory_order_acquire))
                    std::this_thread::yield();

                // All workers increment the same chunk version
                for (int bump = 0; bump < BUMPS_PER_THREAD; ++bump)
                    versions.bump(0);
            });
        }

        // The main thread waits until all four workers have reached the gate.
        while (readyThreads.load(std::memory_order_acquire) < THREAD_COUNT)
            std::this_thread::yield();

        // Open the gate. All workers may now begin incrementing.
        start.store(true, std::memory_order_release);

        for (std::thread& thread : threads)
            thread.join();

        int expectedVersion = 1 + THREAD_COUNT * BUMPS_PER_THREAD;
        REQUIRE(versions.get(0) == expectedVersion);
    }

    TEST_CASE("VersionTable: rejects a non-positive chunk count")
    {
        REQUIRE_THROWS_AS(VersionTable(0), std::invalid_argument);
        REQUIRE_THROWS_AS(VersionTable(-1), std::invalid_argument);
    }

    /************************************ RENDERER TESTS ************************************/
    TEST_CASE("Renderer: injected kernel receives absolute frames and content version")
    {
        VersionTable versions(4);
        versions.bump(2);
        TestRenderKernel kernel;
        Renderer renderer(kernel, versions);
        RenderJob job;
        functions::initializeJob(job, versions, 2);
        std::atomic<bool> stopRequested{false};
        bool expectedCompletion = true;

        SECTION("current job renders") {}
        SECTION("shutdown skips the kernel")
        {
            stopRequested.store(true, std::memory_order_release);
            expectedCompletion = false;
        }
        SECTION("cancelled job skips the kernel")
        {
            job.cancelled.store(true, std::memory_order_relaxed);
            expectedCompletion = false;
        }
        SECTION("obsolete job skips the kernel")
        {
            versions.bump(2);
            expectedCompletion = false;
        }

        constexpr int TILE_INDEX = 3;
        REQUIRE(renderer.renderTile(job, TILE_INDEX, stopRequested) == expectedCompletion);
        REQUIRE(kernel.callCount() == (expectedCompletion ? TILE_FRAMES : 0));
        REQUIRE(job.cancelled.load(std::memory_order_relaxed) == !expectedCompletion);
        REQUIRE(job.tilesRemaining.load(std::memory_order_relaxed) == TILES_PER_CHUNK);
        REQUIRE(job.chunk == 2);
        REQUIRE(job.version == 2);

        for (int frame = 0; frame < CHUNK_FRAMES; ++frame)
        {
            CAPTURE(frame);
            float expected = functions::UNTOUCHED_SAMPLE;

            if (expectedCompletion && frame >= TILE_INDEX * TILE_FRAMES && frame < (TILE_INDEX + 1) * TILE_FRAMES)
                expected = static_cast<float>(2 * CHUNK_FRAMES + frame) / 16384.0f + 2.0f / 128.0f;

            REQUIRE(job.samples[frame] == expected);
        }
    }
    TEST_CASE("Renderer: renders through tile and version-reader interfaces")
    {
        TestVersionReader versions(1000);
        SyntheticRenderKernel kernel(48000, functions::makeTestRenderSettings().workIterations);
        Renderer renderer(kernel, versions);
        const ITileRenderer& tileRenderer = renderer;
        RenderJob job;
        job.chunk = 0;
        job.version = 7;
        job.samples.fill(functions::UNTOUCHED_SAMPLE);
        std::atomic<bool> stopRequested{false};

        REQUIRE(tileRenderer.renderTile(job, 1, stopRequested));
        REQUIRE_FALSE(job.cancelled.load(std::memory_order_relaxed));
        REQUIRE(job.tilesRemaining.load(std::memory_order_relaxed) == TILES_PER_CHUNK);

        for (int frame = 0; frame < CHUNK_FRAMES; ++frame)
        {
            CAPTURE(frame);
            if (frame >= TILE_FRAMES && frame < 2 * TILE_FRAMES)
            {
                REQUIRE(job.samples[frame] != functions::UNTOUCHED_SAMPLE);
                REQUIRE(std::isfinite(job.samples[frame]));
            }
            else
                REQUIRE(job.samples[frame] == functions::UNTOUCHED_SAMPLE);
        }
    }

    TEST_CASE("Renderer: observes version-reader invalidation during rendering")
    {
        TestVersionReader versions(2);
        SyntheticRenderKernel kernel(48000, functions::makeTestRenderSettings().workIterations);
        Renderer renderer(kernel, versions);
        const ITileRenderer& tileRenderer = renderer;
        RenderJob job;
        job.chunk = 0;
        job.version = 7;
        job.samples.fill(functions::UNTOUCHED_SAMPLE);
        std::atomic<bool> stopRequested{false};

        REQUIRE_FALSE(tileRenderer.renderTile(job, 0, stopRequested));
        REQUIRE(job.cancelled.load(std::memory_order_relaxed));
        REQUIRE(job.tilesRemaining.load(std::memory_order_relaxed) == TILES_PER_CHUNK);

        int writtenFrames = 0;
        for (int frame = 0; frame < TILE_FRAMES; ++frame)
            if (job.samples[frame] != functions::UNTOUCHED_SAMPLE)
                ++writtenFrames;

        REQUIRE(writtenFrames > 0);
        REQUIRE(writtenFrames < TILE_FRAMES);
        for (int frame = TILE_FRAMES; frame < CHUNK_FRAMES; ++frame)
            REQUIRE(job.samples[frame] == functions::UNTOUCHED_SAMPLE);
    }

    TEST_CASE("Renderer: final cancellation check rejects a fully written obsolete tile")
    {
        // Allow the entry check and every periodic check to see the current version.
        const int currentReads = 1 + (TILE_FRAMES + CANCELLATION_CHECK_FRAMES - 1) / CANCELLATION_CHECK_FRAMES;
        TestVersionReader versions(currentReads);
        SyntheticRenderKernel kernel(48000, functions::makeTestRenderSettings().workIterations);
        Renderer renderer(kernel, versions);
        RenderJob job;
        job.chunk = 0;
        job.version = 7;
        job.samples.fill(functions::UNTOUCHED_SAMPLE);
        std::atomic<bool> stopRequested{false};

        REQUIRE_FALSE(renderer.renderTile(job, 0, stopRequested));
        REQUIRE(job.cancelled.load(std::memory_order_relaxed));
        REQUIRE(job.tilesRemaining.load(std::memory_order_relaxed) == TILES_PER_CHUNK);

        for (int frame = 0; frame < TILE_FRAMES; ++frame)
            REQUIRE(job.samples[frame] != functions::UNTOUCHED_SAMPLE);

        for (int frame = TILE_FRAMES; frame < CHUNK_FRAMES; ++frame)
            REQUIRE(job.samples[frame] == functions::UNTOUCHED_SAMPLE);
    }

    TEST_CASE("Renderer: rejects invalid tile and chunk indices")
    {
        VersionTable versions(4);
        SyntheticRenderKernel kernel(48000, functions::makeTestRenderSettings().workIterations);
        Renderer renderer(kernel, versions);

        RenderJob job;
        functions::initializeJob(job, versions, 1);

        std::atomic<bool> stopRequested{false};

        REQUIRE_THROWS_AS(renderer.renderTile(job, -1, stopRequested), std::out_of_range);
        REQUIRE_THROWS_AS(renderer.renderTile(job, TILES_PER_CHUNK, stopRequested), std::out_of_range);
        REQUIRE_THROWS_AS(renderer.renderTile(job, TILES_PER_CHUNK + 1, stopRequested), std::out_of_range);
        job.chunk = -1;
        REQUIRE_THROWS_AS(renderer.renderTile(job, 0, stopRequested), std::out_of_range);
        job.chunk = versions.count();
        REQUIRE_THROWS_AS(renderer.renderTile(job, 0, stopRequested), std::out_of_range);
        job.chunk = versions.count() + 1;
        REQUIRE_THROWS_AS(renderer.renderTile(job, 0, stopRequested), std::out_of_range);
    }

    TEST_CASE("Renderer: writes only the requested tile")
    {
        VersionTable versions(4);
        SyntheticRenderKernel kernel(48000, functions::makeTestRenderSettings().workIterations);
        Renderer renderer(kernel, versions);

        RenderJob job;
        functions::initializeJob(job, versions, 1);

        std::atomic<bool> stopRequested{false};
        constexpr int TILE_INDEX = 2;

        bool completed = renderer.renderTile(job, TILE_INDEX, stopRequested);

        REQUIRE(completed);
        REQUIRE_FALSE(job.cancelled.load(std::memory_order_relaxed));

        int firstFrame = TILE_INDEX * TILE_FRAMES;
        int lastFrame = firstFrame + TILE_FRAMES;
        bool generatedNonZeroSample = false;
        
        // below we'll verify that renderTile() modifies exactly the requested tile
        // and produces numerically valid, non-silent samples 
        for (int frame = 0; frame < CHUNK_FRAMES; ++frame)
        {
            CAPTURE(frame);

            if (frame >= firstFrame && frame < lastFrame)
            {
                REQUIRE(job.samples[frame] != functions::UNTOUCHED_SAMPLE);
                REQUIRE(std::isfinite(job.samples[frame]));
                REQUIRE(std::abs(job.samples[frame]) <= 1.0f);

                if (std::abs(job.samples[frame]) > 0.000001f)
                    generatedNonZeroSample = true;
            }
            else
            {
                REQUIRE(job.samples[frame] == functions::UNTOUCHED_SAMPLE);
            }
        }

        REQUIRE(generatedNonZeroSample);
    }

    TEST_CASE("Renderer: identical inputs produce identical output")
    {
        VersionTable versions(4);
        SyntheticRenderKernel kernel(48000, functions::makeTestRenderSettings().workIterations);
        Renderer renderer(kernel, versions);

        RenderJob firstJob;
        RenderJob secondJob;

        functions::initializeJob(firstJob, versions, 1);
        functions::initializeJob(secondJob, versions, 1);

        std::atomic<bool> stopRequested{false};
        constexpr int TILE_INDEX = 3;

        REQUIRE(renderer.renderTile(firstJob, TILE_INDEX, stopRequested));
        REQUIRE(renderer.renderTile(secondJob, TILE_INDEX, stopRequested));

        int firstFrame = TILE_INDEX * TILE_FRAMES;
        int lastFrame = firstFrame + TILE_FRAMES;

        for (int frame = firstFrame; frame < lastFrame; ++frame)
        {
            CAPTURE(frame);
            REQUIRE(firstJob.samples[frame] == secondJob.samples[frame]);
        }
    }

    TEST_CASE("Renderer: stop request cancels before rendering")
    {
        VersionTable versions(4);
        SyntheticRenderKernel kernel(48000, functions::makeTestRenderSettings().workIterations);
        Renderer renderer(kernel, versions);

        RenderJob job;
        functions::initializeJob(job, versions, 1);

        std::atomic<bool> stopRequested{true};

        REQUIRE_FALSE(renderer.renderTile(job, 0, stopRequested));
        REQUIRE(job.cancelled.load(std::memory_order_relaxed));

        for (float sample : job.samples)
            REQUIRE(sample == functions::UNTOUCHED_SAMPLE);
    }

    TEST_CASE("Renderer: obsolete version cancels before rendering")
    {
        VersionTable versions(4);
        SyntheticRenderKernel kernel(48000, functions::makeTestRenderSettings().workIterations);
        Renderer renderer(kernel, versions);

        RenderJob job;
        functions::initializeJob(job, versions, 1);

        // Simulate an edit *after* the job was created.
        versions.bump(job.chunk);

        std::atomic<bool> stopRequested{false};

        REQUIRE_FALSE(renderer.renderTile(job, 0, stopRequested));
        REQUIRE(job.cancelled.load(std::memory_order_relaxed));

        for (float sample : job.samples)
            REQUIRE(sample == functions::UNTOUCHED_SAMPLE);
    }

    TEST_CASE("Renderer: different tiles can render concurrently")
    {
        // Same logic as TEST_CASE - VersionTable: concurrent bumps are not lost
        VersionTable versions(4);
        SyntheticRenderKernel kernel(48000, functions::makeTestRenderSettings().workIterations);
        Renderer renderer(kernel, versions);

        RenderJob job;
        functions::initializeJob(job, versions, 1);

        std::atomic<bool> stopRequested{false};
        std::atomic<int>  readyWorkers{0};
        std::atomic<bool> startRendering{false};

        std::array<std::thread, TILES_PER_CHUNK> workers;
        std::array<bool, TILES_PER_CHUNK> completed{};

        for (int tile = 0; tile < TILES_PER_CHUNK; ++tile)
        {
            workers[tile] = std::thread([&renderer, &job, &stopRequested, &completed, &readyWorkers, &startRendering, tile]
            {
                readyWorkers.fetch_add(1, std::memory_order_release);

                while (!startRendering.load(std::memory_order_acquire))
                    std::this_thread::yield();

                completed[tile] = renderer.renderTile(job, tile, stopRequested);
            });
        }

        while (readyWorkers.load(std::memory_order_acquire) < TILES_PER_CHUNK)
            std::this_thread::yield();

        startRendering.store(true, std::memory_order_release);

        for (std::thread& worker : workers)
            worker.join();

        for (int tile = 0; tile < TILES_PER_CHUNK; ++tile)
        {
            CAPTURE(tile);
            REQUIRE(completed[tile]);
        }

        REQUIRE_FALSE(job.cancelled.load(std::memory_order_relaxed));

        for (int frame = 0; frame < CHUNK_FRAMES; ++frame)
        {
            CAPTURE(frame);
            REQUIRE(job.samples[frame] != functions::UNTOUCHED_SAMPLE);
            REQUIRE(std::isfinite(job.samples[frame]));
            REQUIRE(std::abs(job.samples[frame]) <= 1.0f);
        }
    }

    #ifdef enable_benchmarks
    TEST_CASE("Renderer: Check renderTile() execution time / benchmark")
    {
        VersionTable versions(4);
        RenderSettings settings = functions::makeTestRenderSettings(300);
        SyntheticRenderKernel kernel(48000, settings.workIterations);
        Renderer renderer(kernel, versions);
        std::atomic<bool> stopRequested{false};

        RenderJob job;
        functions::initializeJob(job, versions, 1);

        benchmarks::Benchmark benchmark(100, 10, 10);
        benchmarks::BenchmarkResult result = benchmark.run([&](std::size_t operation) -> double
        {
            int tileIndex = static_cast<int>(operation % TILES_PER_CHUNK);

            if (!renderer.renderTile(job, tileIndex, stopRequested))
                throw std::runtime_error("renderTile() was cancelled");

            int sampleIndex = tileIndex * TILE_FRAMES + TILE_FRAMES / 2;
            double sample = job.samples[sampleIndex];

            return sample * sample;
        });
        

        REQUIRE(std::isfinite(result.operationsPerSecond));
        REQUIRE(std::isfinite(result.nanosecondsPerOperation));
        REQUIRE(result.operationsPerSecond > 0.0);
        REQUIRE(result.nanosecondsPerOperation > 0.0);
        REQUIRE(result.checksum > 0.0);
        double renderTimeInUs = result.nanosecondsPerOperation / 1000.0;
        std::cout << "\n*--------------------------------* \n"
                << "|      Renderer (renderTile())   | \n"
                << "*--------------------------------* \n"
                << "Work iterations:       " << settings.workIterations << '\n'
                << "Operations per second: " << result.operationsPerSecond << '\n'
                << "Average tile time:     " << renderTimeInUs << " us\n"
                << "Checksum:              " << result.checksum << '\n';
        
    }
    #endif // enable_benchmarks

} // namespace anasa