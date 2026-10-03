#include <memory>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "scheduler/ActiveRenderJobs.hpp"

namespace anasa
{
    TEST_CASE("ActiveRenderJobs: rejects invalid storage configuration")
    {
        int channelCount = 2;
        int chunkCount = 2;

        SECTION("Zero channels") { channelCount = 0; }
        SECTION("Negative channels") { channelCount = -1; }
        SECTION("Zero chunks") { chunkCount = 0; }
        SECTION("Negative chunks") { chunkCount = -1; }

        REQUIRE_THROWS_AS(ActiveRenderJobs(channelCount, chunkCount), std::invalid_argument);
    }

    TEST_CASE("ActiveRenderJobs: tracks chunks independently and releases completed jobs once")
    {
        ActiveRenderJobs jobs(2, 2);
        REQUIRE_FALSE(jobs.hasCurrent(0, 1));

        const std::shared_ptr<RenderJob> first = jobs.create(0, 3);
        const std::shared_ptr<RenderJob> second = jobs.create(1, 1);

        REQUIRE(first->chunk == 0);
        REQUIRE(first->version == 3);
        REQUIRE(first->samples.channelCount() == 2);
        REQUIRE(first->samples.frameCount() == CHUNK_FRAMES);
        REQUIRE(first->tilesRemaining.load() == TILES_PER_CHUNK);
        REQUIRE(jobs.hasCurrent(0, 3));
        REQUIRE_FALSE(jobs.hasCurrent(0, 2));
        REQUIRE(jobs.hasCurrent(1, 1));

        // Simulate a job delivered by the executor's completion queue.
        first->tilesRemaining.store(0);
        REQUIRE(jobs.finish(first));
        REQUIRE_FALSE(jobs.hasCurrent(0, 3));
        REQUIRE_FALSE(jobs.finish(first));
        REQUIRE(jobs.hasCurrent(1, 1));
        REQUIRE_FALSE(second->cancelled.load());
    }

    TEST_CASE("ActiveRenderJobs: nonmatching completions cannot release a current job")
    {
        ActiveRenderJobs jobs(1, 2);
        std::shared_ptr<RenderJob> current = jobs.create(0, 1);
        std::shared_ptr<RenderJob> completed = std::make_shared<RenderJob>(1);
        completed->chunk = 0;
        completed->version = 1;

        SECTION("Null completion") { completed.reset(); }
        SECTION("Negative chunk") { completed->chunk = -1; }
        SECTION("Chunk past the end") { completed->chunk = 2; }
        SECTION("Untracked chunk") { completed->chunk = 1; }
        SECTION("Different identity with the same chunk and version") {}
        SECTION("Replaced job with the same version")
        {
            completed = current;
            current = jobs.create(0, 1);
            REQUIRE(completed->cancelled.load());
        }

        if (completed != nullptr)
            completed->tilesRemaining.store(0);

        REQUIRE_FALSE(jobs.finish(completed));
        REQUIRE(jobs.hasCurrent(0, 1));
        REQUIRE_FALSE(current->cancelled.load());

        current->tilesRemaining.store(0);
        REQUIRE(jobs.finish(current));
    }

    TEST_CASE("ActiveRenderJobs: cancellation retains ownership until matching completion")
    {
        ActiveRenderJobs jobs(1, 2);
        std::shared_ptr<RenderJob> completed = jobs.create(0, 1);
        const std::shared_ptr<RenderJob> other = jobs.create(1, 1);
        std::weak_ptr<RenderJob> lifetime = completed;

        SECTION("Scheduler requests cancellation") { jobs.cancel(0); }
        SECTION("Executor reports cancellation") { completed->cancelled.store(true); }

        REQUIRE_FALSE(jobs.hasCurrent(0, 1));
        REQUIRE(jobs.hasCurrent(1, 1));
        REQUIRE_FALSE(other->cancelled.load());

        completed.reset();
        REQUIRE_FALSE(lifetime.expired());

        completed = lifetime.lock();
        completed->tilesRemaining.store(0);
        REQUIRE_FALSE(jobs.finish(completed));
        completed.reset();
        REQUIRE(lifetime.expired());

        const std::shared_ptr<RenderJob> retry = jobs.create(0, 1);
        REQUIRE(jobs.hasCurrent(0, 1));
        REQUIRE_FALSE(retry->cancelled.load());
    }

    TEST_CASE("ActiveRenderJobs: shutdown cancels and forgets jobs without destroying external owners")
    {
        ActiveRenderJobs jobs(1, 2);
        auto oldJob = jobs.create(0, 1);
        auto otherJob = jobs.create(1, 1);
        std::weak_ptr<RenderJob> otherLifetime = otherJob;

        jobs.cancelAll();
        jobs.cancelAll();

        REQUIRE(oldJob->cancelled.load());
        REQUIRE(otherJob->cancelled.load());
        REQUIRE_FALSE(jobs.hasCurrent(0, 1));
        REQUIRE_FALSE(jobs.hasCurrent(1, 1));
        otherJob.reset();
        REQUIRE(otherLifetime.expired());

        // An executor may still own oldJob when a new scheduling session starts.
        const auto newJob = jobs.create(0, 1);
        oldJob->tilesRemaining.store(0);
        REQUIRE_FALSE(jobs.finish(oldJob));
        REQUIRE(jobs.hasCurrent(0, 1));
        REQUIRE_FALSE(newJob->cancelled.load());
    }
} // namespace anasa