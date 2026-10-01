#include <stdexcept>
#include <limits>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "functions/Functions.hpp"
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
        SECTION("Zero channels") { settings.audio.channelCount = 0; }
        SECTION("Negative channels") { settings.audio.channelCount = -1; }
        SECTION("Zero block frames") { settings.audio.audioBlockFrames = 0; }
        SECTION("Negative block frames") { settings.audio.audioBlockFrames = -1; }
        SECTION("Oversized block frames"){settings.audio.audioBlockFrames = std::numeric_limits<int>::max();}
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

    TEST_CASE("Engine: configured channels reach the simulated callback")
    {
        EngineSettings settings;
        settings.timelineInSeconds = 1;
        settings.audio.channelCount = GENERATE(1, 2, 6, 16);
        settings.render.workIterations = 0;

        Engine engine(settings);

        engine.start();
        REQUIRE(engine.post({CommandType::Play, 0, 0}));

        const bool advanced = functions::waitUntil([&]
        {
            return engine.playbackSnapshot().nextUnconsumedFrame >=
                   settings.audio.audioBlockFrames;
        });

        engine.stop();

        REQUIRE(advanced);

        const EngineMetrics metrics = engine.metrics();

        REQUIRE(metrics.callbacks > 0);
        REQUIRE(metrics.underruns < metrics.callbacks);
    }
} // namespace anasa