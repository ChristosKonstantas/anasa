#include <array>
#include <atomic>
#include <memory>
#include <thread>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "execution/RenderTaskProcessor.hpp"
#include "functions/Functions.hpp"
#include "TestTileRenderer.hpp"

namespace anasa
{
    TEST_CASE("RenderTaskProcessor: accounts for every tile across rendering outcomes")
    {
        const int channelCount = GENERATE(1, 2, 6, 16);
        RenderOutcome outcome = RenderOutcome::Complete;
        std::atomic<bool> stopRequested{false};
        bool alreadyCancelled = false;

        SECTION("successful rendering") {}
        SECTION("false result cancels the job") { outcome = RenderOutcome::Cancel; }
        SECTION("exception cancels the job") { outcome = RenderOutcome::Throw; }
        SECTION("shutdown requested") { stopRequested.store(true, std::memory_order_release); }
        SECTION("job already cancelled") { alreadyCancelled = true; }

        TestTileRenderer renderer(outcome);
        RenderTaskProcessor processor(renderer);
        VersionTable versions(1);
        std::shared_ptr<RenderJob> job = functions::makeTestRenderJob(versions, 0, channelCount);
        job->cancelled.store(alreadyCancelled, std::memory_order_relaxed);

        const bool expectedCancellation = outcome != RenderOutcome::Complete || stopRequested.load(std::memory_order_acquire) || alreadyCancelled;

        for (int tile = 0; tile < TILES_PER_CHUNK; ++tile)
        {
            CAPTURE(tile);
            const bool jobCompleted = processor.process({job, tile}, stopRequested);

            REQUIRE(jobCompleted == (tile == TILES_PER_CHUNK - 1));
            REQUIRE(job->tilesRemaining.load(std::memory_order_relaxed) == TILES_PER_CHUNK - tile - 1);
            REQUIRE(job->cancelled.load(std::memory_order_relaxed) == expectedCancellation);
            REQUIRE(renderer.calls[tile].load(std::memory_order_relaxed) == 1);
        }

        REQUIRE(job->chunk == 0);
        REQUIRE(job->version == 1);

        for (int channel = 0; channel < channelCount; ++channel)
        {
            for (int frame = 0; frame < CHUNK_FRAMES; ++frame)
            {
                CAPTURE(channel, frame);
                const float expected = expectedCancellation ? functions::UNTOUCHED_SAMPLE : TestTileRenderer::sampleForTile(frame / TILE_FRAMES, channel);
                REQUIRE(job->samples[channel][frame] == expected);
            }
        }
    }

    TEST_CASE("RenderTaskProcessor: concurrent tiles report one completion with all samples visible")
    {
        const int channelCount = GENERATE(1, 2, 6, 16);
        TestTileRenderer renderer(RenderOutcome::Complete);
        RenderTaskProcessor processor(renderer);
        VersionTable versions(1);
        std::shared_ptr<RenderJob> job = functions::makeTestRenderJob(versions, 0, channelCount);
        std::atomic<bool> stopRequested{false};
        std::atomic<int> readyWorkers{0};
        std::atomic<bool> startProcessing{false};
        std::array<std::thread, TILES_PER_CHUNK> workers;
        std::array<bool, TILES_PER_CHUNK> completed{};
        AudioBuffer completedSamples(channelCount, CHUNK_FRAMES);

        for (int tile = 0; tile < TILES_PER_CHUNK; ++tile)
        {
            workers[tile] = std::thread([&processor, &job, &stopRequested, &readyWorkers, &startProcessing, &completed, &completedSamples, tile]
            {
                readyWorkers.fetch_add(1, std::memory_order_release);

                while (!startProcessing.load(std::memory_order_acquire))
                    std::this_thread::yield();

                completed[tile] = processor.process({job, tile}, stopRequested);

                // The final tile must see all samples before the workers are joined.
                if (completed[tile])
                    completedSamples = job->samples;
            });
        }

        while (readyWorkers.load(std::memory_order_acquire) < TILES_PER_CHUNK)
            std::this_thread::yield();

        startProcessing.store(true, std::memory_order_release);

        for (std::thread& worker : workers)
            worker.join();

        int completionCount = 0;

        for (int tile = 0; tile < TILES_PER_CHUNK; ++tile)
        {
            if (completed[tile])
                ++completionCount;

            REQUIRE(renderer.calls[tile].load(std::memory_order_relaxed) == 1);
        }

        REQUIRE(completionCount == 1);
        REQUIRE(job->tilesRemaining.load(std::memory_order_acquire) == 0);
        REQUIRE_FALSE(job->cancelled.load(std::memory_order_relaxed));

        for (int channel = 0; channel < channelCount; ++channel)
        {
            for (int frame = 0; frame < CHUNK_FRAMES; ++frame)
            {
                CAPTURE(channel, frame);
                REQUIRE(completedSamples[channel][frame] == TestTileRenderer::sampleForTile(frame / TILE_FRAMES, channel));
            }
        }
    }
} // namespace anasa