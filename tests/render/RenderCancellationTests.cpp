#include <atomic>

#include <catch2/catch_test_macros.hpp>

#include "functions/Functions.hpp"
#include "render/RenderCancellation.hpp"
#include "render/VersionTable.hpp"

namespace anasa
{
    TEST_CASE("RenderCancellation: checks shutdown job cancellation and content version")
    {
        VersionTable versions(4);
        RenderCancellation cancellation(versions);
        RenderJob job;
        functions::initializeJob(job, versions, 1);
        std::atomic<bool> stopRequested{false};
        bool expected = true;

        SECTION("current job continues") { expected = false; }
        SECTION("shutdown cancels") { stopRequested.store(true, std::memory_order_release); }
        SECTION("explicit cancellation cancels") { job.cancelled.store(true, std::memory_order_relaxed); }
        SECTION("edited content cancels") { versions.bump(job.chunk); }
        SECTION("another chunk's edit does not cancel")
        {
            versions.bump(0);
            expected = false;
        }

        REQUIRE(cancellation.shouldCancel(job, stopRequested) == expected);
    }
} // namespace anasa