#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "playback/BackgroundRenderGate.hpp"

namespace anasa
{
    TEST_CASE("BackgroundRenderGate: rejects invalid thresholds")
    {
        REQUIRE_THROWS_AS(BackgroundRenderGate(-1, 8), std::invalid_argument);
        REQUIRE_THROWS_AS(BackgroundRenderGate(4, 3), std::invalid_argument);
    }

    TEST_CASE("BackgroundRenderGate: retains its decision inside the watermarks")
    {
        BackgroundRenderGate gate(4, 8);
        REQUIRE(gate.allowsBackground());

        gate.observe(4);
        REQUIRE(gate.allowsBackground());

        gate.observe(3);
        REQUIRE_FALSE(gate.allowsBackground());

        gate.observe(4);
        REQUIRE_FALSE(gate.allowsBackground());

        gate.observe(7);
        REQUIRE_FALSE(gate.allowsBackground());

        gate.observe(8);
        REQUIRE(gate.allowsBackground());

        gate.observe(7);
        REQUIRE(gate.allowsBackground());

        gate.observe(4);
        REQUIRE(gate.allowsBackground());

        gate.observe(3);
        REQUIRE_FALSE(gate.allowsBackground());
    }

    TEST_CASE("BackgroundRenderGate: reset restores initial permission")
    {
        BackgroundRenderGate gate(4, 8);
        gate.observe(0);
        REQUIRE_FALSE(gate.allowsBackground());

        gate.reset();
        REQUIRE(gate.allowsBackground());

        gate.observe(6);
        REQUIRE(gate.allowsBackground());

        gate.observe(0);
        REQUIRE_FALSE(gate.allowsBackground());
    }

    TEST_CASE("BackgroundRenderGate: equal watermarks use one boundary")
    {
        BackgroundRenderGate gate(4, 4);

        gate.observe(3);
        REQUIRE_FALSE(gate.allowsBackground());

        gate.observe(4);
        REQUIRE(gate.allowsBackground());

        gate.observe(3);
        REQUIRE_FALSE(gate.allowsBackground());
    }

    TEST_CASE("BackgroundRenderGate: zero watermarks allow nonnegative headroom")
    {
        BackgroundRenderGate gate(0, 0);
        gate.observe(GENERATE(0, 1, 16));
        REQUIRE(gate.allowsBackground());
    }
} // namespace anasa