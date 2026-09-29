#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <stdexcept>

#include "playback/PlaybackProtocol.hpp"

namespace anasa
{
    TEST_CASE("PlaybackProtocol: rejects invalid timeline lengths")
    {
        SharedState state;

        REQUIRE_THROWS_AS(PlaybackProtocol(0, state), std::invalid_argument);
        REQUIRE_THROWS_AS(PlaybackProtocol(-1, state), std::invalid_argument);
    }

    TEST_CASE("PlaybackProtocol: reset publishes a generation without acknowledging it")
    {
        const bool playing = GENERATE(false, true);
        const bool suspendPlayback = GENERATE(false, true);
        const int targetFrame = GENERATE(0, 256, 1024);
        SharedState state;
        PlaybackProtocol protocol(1024, state);
        state.playing.store(playing, std::memory_order_relaxed);
        state.nextUnconsumedFrame.store(768, std::memory_order_relaxed);

        protocol.beginGeneration(targetFrame, suspendPlayback);

        REQUIRE(state.generation.load() == 2);
        REQUIRE(state.targetFrame.load() == targetFrame);
        REQUIRE(state.playing.load() == (playing && !suspendPlayback));
        REQUIRE(state.nextUnconsumedFrame.load() == 768);
        REQUIRE(state.audioCursorGeneration.load() == 1);
        REQUIRE(protocol.currentFrame() == targetFrame);
    }

    TEST_CASE("PlaybackProtocol: only the latest acknowledgement makes cursor progress authoritative")
    {
        SharedState state;
        PlaybackProtocol protocol(1024, state);
        state.nextUnconsumedFrame.store(768, std::memory_order_relaxed);
        REQUIRE(protocol.currentFrame() == 768);

        protocol.beginGeneration(256, true);

        // Model progress from an old callback that finishes after the reset.
        state.nextUnconsumedFrame.store(896, std::memory_order_release);
        REQUIRE(protocol.currentFrame() == 256);

        protocol.beginGeneration(512, true);
        REQUIRE(state.generation.load() == 3);
        REQUIRE(protocol.currentFrame() == 512);

        // An acknowledgement of the intermediate generation is still obsolete.
        state.nextUnconsumedFrame.store(384, std::memory_order_relaxed);
        state.audioCursorGeneration.store(2, std::memory_order_release);
        REQUIRE(protocol.currentFrame() == 512);

        state.nextUnconsumedFrame.store(512, std::memory_order_relaxed);
        state.audioCursorGeneration.store(3, std::memory_order_release);
        REQUIRE(protocol.currentFrame() == 512);

        state.nextUnconsumedFrame.store(640, std::memory_order_release);
        REQUIRE(protocol.currentFrame() == 640);
    }

    TEST_CASE("PlaybackProtocol: acknowledged progress is bounded by the timeline")
    {
        SharedState state;
        PlaybackProtocol protocol(1024, state);
        int cursor = 384;
        int expectedFrame = 384;

        SECTION("inside the timeline") {}
        SECTION("before the timeline")
        {
            cursor = -128;
            expectedFrame = 0;
        }
        SECTION("at the end")
        {
            cursor = 1024;
            expectedFrame = 1024;
        }
        SECTION("past the end")
        {
            cursor = 1152;
            expectedFrame = 1024;
        }

        state.nextUnconsumedFrame.store(cursor, std::memory_order_release);
        REQUIRE(protocol.currentFrame() == expectedFrame);
    }
} // namespace anasa