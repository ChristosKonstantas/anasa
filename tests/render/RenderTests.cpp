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

#ifdef enable_benchmarks
#include "benchmarks/Benchmark.hpp"
#endif

#include "functions/Functions.hpp"

namespace anasa
{
    /********************** VERSION TABLE TESTS **********************/
    TEST_CASE("VersionTable: initializes every chunk with version one")
    {
        VersionTable versions(4);

        REQUIRE(versions.count() == 4);

        for (int chunk = 0; chunk < versions.count(); ++chunk)
            REQUIRE(versions.get(chunk) == 1);
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

    TEST_CASE("Renderer: rejects invalid settings")
    {
        VersionTable versions(1);
        RenderSettings settings = functions::makeTestRenderSettings();

        REQUIRE_THROWS_AS(Renderer(0, settings, versions), std::invalid_argument);
        REQUIRE_THROWS_AS(Renderer(-48000, settings, versions), std::invalid_argument);

        settings.workIterations = -1;

        REQUIRE_THROWS_AS(Renderer(48000, settings, versions), std::invalid_argument);
    }

    TEST_CASE("Renderer: rejects invalid tile and chunk indices")
    {
        VersionTable versions(4);
        RenderSettings settings = functions::makeTestRenderSettings();
        Renderer renderer(48000, settings, versions);

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
        RenderSettings settings = functions::makeTestRenderSettings();
        Renderer renderer(48000, settings, versions);

        RenderJob job;
        functions::initializeJob(job, versions, 1);

        std::atomic<bool> stopRequested{false};
        constexpr int TILE_INDEX = 2;

        bool completed = renderer.renderTile(job, TILE_INDEX, stopRequested);

        REQUIRE(completed);
        REQUIRE_FALSE(job.cancelled.load(std::memory_order_acquire));

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
        RenderSettings settings = functions::makeTestRenderSettings();
        Renderer renderer(48000, settings, versions);

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
        RenderSettings settings = functions::makeTestRenderSettings();
        Renderer renderer(48000, settings, versions);

        RenderJob job;
        functions::initializeJob(job, versions, 1);

        std::atomic<bool> stopRequested{true};

        REQUIRE_FALSE(renderer.renderTile(job, 0, stopRequested));
        REQUIRE(job.cancelled.load(std::memory_order_acquire));

        for (float sample : job.samples)
            REQUIRE(sample == functions::UNTOUCHED_SAMPLE);
    }

    TEST_CASE("Renderer: obsolete version cancels before rendering")
    {
        VersionTable versions(4);
        RenderSettings settings = functions::makeTestRenderSettings();
        Renderer renderer(48000, settings, versions);

        RenderJob job;
        functions::initializeJob(job, versions, 1);

        // Simulate an edit *after* the job was created.
        versions.bump(job.chunk);

        std::atomic<bool> stopRequested{false};

        REQUIRE_FALSE(renderer.renderTile(job, 0, stopRequested));
        REQUIRE(job.cancelled.load(std::memory_order_acquire));

        for (float sample : job.samples)
            REQUIRE(sample == functions::UNTOUCHED_SAMPLE);
    }

    TEST_CASE("Renderer: different tiles can render concurrently")
    {
        // Same logic as TEST_CASE - VersionTable: concurrent bumps are not lost
        VersionTable versions(4);
        RenderSettings settings = functions::makeTestRenderSettings();
        Renderer renderer(48000, settings, versions);

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

        REQUIRE_FALSE(job.cancelled.load(std::memory_order_acquire));

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
        Renderer renderer(48000, settings, versions);
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