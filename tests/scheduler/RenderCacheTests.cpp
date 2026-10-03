#include <limits>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "render/VersionTable.hpp"
#include "scheduler/RenderCache.hpp"

namespace anasa
{
    TEST_CASE("RenderCache: validates storage configuration before allocating samples")
    {
        VersionTable versions(1);
        int channelCount = 2;
        int totalFrames = CHUNK_FRAMES;

        SECTION("Zero channels") { channelCount = 0; }
        SECTION("Negative channels") { channelCount = -1; }
        SECTION("Empty timeline") { totalFrames = 0; }
        SECTION("Negative timeline") { totalFrames = std::numeric_limits<int>::min(); }
        SECTION("Mismatched version count") { totalFrames = 2 * CHUNK_FRAMES; }

        REQUIRE_THROWS_AS(RenderCache(channelCount, totalFrames, versions), std::invalid_argument);
    }

    TEST_CASE("RenderCache: owns a multichannel snapshot and exposes it through the reader")
    {
        const int channelCount = GENERATE(1, 2, 6, 16);
        VersionTable versions(2);
        RenderCache cache(channelCount, CHUNK_FRAMES + 128, versions);
        const IRenderCacheReader& reader = cache;

        REQUIRE(reader.chunkCount() == 2);
        REQUIRE(reader.findCurrent(0) == nullptr);
        REQUIRE(reader.findCurrent(1) == nullptr);

        AudioBuffer samples(channelCount, CHUNK_FRAMES);

        for (int channel = 0; channel < channelCount; ++channel)
        {
            for (int frame = 0; frame < CHUNK_FRAMES; ++frame)
                samples[channel][frame] = static_cast<float>(channel * CHUNK_FRAMES + frame);
        }

        REQUIRE(cache.store(1, versions.get(1), samples));
        const AudioBuffer* stored = reader.findCurrent(1);
        REQUIRE(stored != nullptr);
        REQUIRE(stored->channelCount() == channelCount);
        REQUIRE(stored->frameCount() == CHUNK_FRAMES);

        for (int channel = 0; channel < channelCount; ++channel)
        {
            for (int frame = 0; frame < CHUNK_FRAMES; ++frame)
                REQUIRE((*stored)[channel][frame] == samples[channel][frame]);
        }

        samples[0][0] = -1.0f;
        REQUIRE((*stored)[0][0] == 0.0f);
        REQUIRE(reader.findCurrent(0) == nullptr);
    }

    TEST_CASE("RenderCache: an edit invalidates only its chunk and rejects stale completion")
    {
        VersionTable versions(2);
        RenderCache cache(2, 2 * CHUNK_FRAMES, versions);
        AudioBuffer samples(2, CHUNK_FRAMES);

        REQUIRE(cache.store(0, 1, samples));
        REQUIRE(cache.store(1, 1, samples));
        const int newVersion = versions.bump(1);

        REQUIRE(cache.findCurrent(0) != nullptr);
        REQUIRE(cache.findCurrent(1) == nullptr);
        REQUIRE_FALSE(cache.store(1, 1, samples));
        REQUIRE(cache.findCurrent(1) == nullptr);

        REQUIRE(cache.store(1, newVersion, samples));
        REQUIRE(cache.findCurrent(1) != nullptr);
    }

    TEST_CASE("RenderCache: rejected storage leaves current content unchanged")
    {
        VersionTable versions(2);
        RenderCache cache(2, 2 * CHUNK_FRAMES, versions);
        AudioBuffer samples(2, CHUNK_FRAMES);
        samples[0][0] = 0.25f;
        samples[1][CHUNK_FRAMES - 1] = 0.75f;
        REQUIRE(cache.store(0, 1, samples));

        int chunk = 0;
        int version = 1;
        samples[0][0] = -1.0f;
        samples[1][CHUNK_FRAMES - 1] = -1.0f;

        SECTION("Negative chunk") { chunk = -1; }
        SECTION("Chunk past the end") { chunk = 2; }
        SECTION("Old version") { version = 0; }
        SECTION("Future version") { version = 2; }
        SECTION("Empty storage") { samples = AudioBuffer{}; }
        SECTION("Wrong channels") { samples = AudioBuffer(1, CHUNK_FRAMES); }
        SECTION("Short chunk") { samples = AudioBuffer(2, CHUNK_FRAMES - 1); }
        SECTION("Oversized chunk") { samples = AudioBuffer(2, CHUNK_FRAMES + 1); }

        REQUIRE_FALSE(cache.store(chunk, version, samples));
        REQUIRE(cache.findCurrent(-1) == nullptr);
        REQUIRE(cache.findCurrent(2) == nullptr);
        const AudioBuffer* stored = cache.findCurrent(0);
        REQUIRE(stored != nullptr);
        REQUIRE((*stored)[0][0] == 0.25f);
        REQUIRE((*stored)[1][CHUNK_FRAMES - 1] == 0.75f);
    }
} // namespace anasa