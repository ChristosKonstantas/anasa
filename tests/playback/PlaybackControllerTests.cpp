#include <atomic>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "playback/PlaybackController.hpp"
#include "playback/PlaybackProtocol.hpp"

namespace anasa
{
    TEST_CASE("PlaybackController: rejects a negative prebuffer threshold")
    {
        std::atomic<bool> playing{false};
        REQUIRE_THROWS_AS(PlaybackController(-1, true, playing), std::invalid_argument);
    }

    TEST_CASE("PlaybackController: starts at the threshold and keeps playing when lead falls")
    {
        const int prebufferBlocks = GENERATE(0, 1, 16);
        std::atomic<bool> playing{false};
        PlaybackController controller(prebufferBlocks, true, playing);

        controller.startIfReady(prebufferBlocks, true);
        REQUIRE_FALSE(playing.load());
        REQUIRE(controller.requestPlay());
        REQUIRE_FALSE(controller.requestPlay());
        REQUIRE(controller.playRequested());
        REQUIRE_FALSE(playing.load());

        if (prebufferBlocks > 0)
        {
            controller.startIfReady(prebufferBlocks - 1, false);
            REQUIRE_FALSE(playing.load());
        }

        controller.startIfReady(prebufferBlocks, false);
        REQUIRE(playing.load());

        controller.startIfReady(0, false);
        REQUIRE(playing.load());
        REQUIRE(controller.playRequested());
    }

    TEST_CASE("PlaybackController: a fully published remainder can start below the threshold")
    {
        std::atomic<bool> playing{false};
        PlaybackController controller(16, true, playing);
        REQUIRE(controller.requestPlay());

        controller.startIfReady(1, false);
        REQUIRE_FALSE(playing.load());

        controller.startIfReady(1, true);
        REQUIRE(playing.load());
    }

    TEST_CASE("PlaybackController: pause cancels either waiting or active playback")
    {
        const bool active = GENERATE(false, true);
        std::atomic<bool> playing{false};
        PlaybackController controller(2, true, playing);
        REQUIRE(controller.requestPlay());
        controller.startIfReady(active ? 2 : 0, false);
        REQUIRE(playing.load() == active);

        REQUIRE(controller.pause());
        REQUIRE_FALSE(controller.playRequested());
        REQUIRE_FALSE(playing.load());
        REQUIRE_FALSE(controller.pause());

        controller.startIfReady(2, true);
        REQUIRE_FALSE(playing.load());

        REQUIRE(controller.requestPlay());
        controller.startIfReady(2, false);
        REQUIRE(playing.load());
    }

    TEST_CASE("PlaybackController: generation resets preserve play intent and follow the rebuffer policy")
    {
        const bool rebufferOnEdit = GENERATE(false, true);
        SharedState state;
        PlaybackProtocol protocol(4096, state);
        PlaybackController controller(2, rebufferOnEdit, state.playing);
        REQUIRE(controller.requestPlay());
        controller.startIfReady(2, false);
        REQUIRE(state.playing.load());

        bool suspendPlayback = controller.shouldRebufferOnEdit();
        REQUIRE(suspendPlayback == rebufferOnEdit);
        SECTION("Seek always suspends consumption") { suspendPlayback = true; }
        SECTION("Edit follows the configured policy") {}

        protocol.beginGeneration(128, suspendPlayback);
        REQUIRE(controller.playRequested());
        REQUIRE(state.playing.load() == !suspendPlayback);

        controller.startIfReady(1, false);
        REQUIRE(state.playing.load() == !suspendPlayback);

        controller.startIfReady(2, false);
        REQUIRE(state.playing.load());

        REQUIRE(controller.pause());
        REQUIRE_FALSE(controller.shouldRebufferOnEdit());
        protocol.beginGeneration(256, controller.shouldRebufferOnEdit());
        controller.startIfReady(2, true);
        REQUIRE_FALSE(controller.playRequested());
        REQUIRE_FALSE(state.playing.load());
    }

    TEST_CASE("PlaybackController: lifecycle reset leaves playback permission to the owner")
    {
        const bool active = GENERATE(false, true);
        std::atomic<bool> playing{false};
        PlaybackController controller(2, true, playing);
        REQUIRE(controller.requestPlay());
        controller.startIfReady(active ? 2 : 0, false);

        controller.resetPlayRequest();
        REQUIRE_FALSE(controller.playRequested());
        REQUIRE(playing.load() == active);
    }
} // namespace anasa