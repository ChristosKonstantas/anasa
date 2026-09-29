#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "engine/Engine.hpp"

namespace anasa
{
    TEST_CASE("Engine: rejects invalid or overflowing timeline settings")
    {
        EngineSettings settings;

        SECTION("Zero duration") { settings.timelineInSeconds = 0; }
        SECTION("Negative duration") { settings.timelineInSeconds = -1; }
        SECTION("Zero sample rate") { settings.audio.sampleRate = 0; }
        SECTION("Negative sample rate") { settings.audio.sampleRate = -1; }
        SECTION("Frame count exceeds int") { settings.timelineInSeconds = 50000; }
        SECTION("Overflow could appear as a small positive timeline") { settings.timelineInSeconds = 89480; }

        REQUIRE_THROWS_AS(Engine(settings), std::invalid_argument);
    }

    TEST_CASE("Engine: valid settings construct a stopped pipeline")
    {
        EngineSettings settings;
        settings.timelineInSeconds = 1;
        Engine engine(settings);

        REQUIRE_FALSE(engine.playbackSnapshot().playing);
        REQUIRE_FALSE(engine.post({CommandType::Play, 0, 0}));
        REQUIRE(engine.metrics().callbacks == 0);
    }
} // namespace anasa