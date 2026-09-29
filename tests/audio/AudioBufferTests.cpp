#include <utility>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "audio/AudioBuffer.hpp"

namespace anasa
{
    TEST_CASE("AudioBuffer: default storage is empty")
    {
        AudioBuffer buffer;
        REQUIRE(buffer.channelCount() == 0);
        REQUIRE(buffer.frameCount() == 0);
    }

    TEST_CASE("AudioBuffer: rejects non-positive dimensions")
    {
        REQUIRE_THROWS_AS(AudioBuffer(0, 4), std::invalid_argument);
        REQUIRE_THROWS_AS(AudioBuffer(-1, 4), std::invalid_argument);
        REQUIRE_THROWS_AS(AudioBuffer(2, 0), std::invalid_argument);
        REQUIRE_THROWS_AS(AudioBuffer(2, -1), std::invalid_argument);
    }

    TEST_CASE("AudioBuffer: channels have independent initially silent samples")
    {
        const int channelCount = GENERATE(1, 2, 6, 16);
        AudioBuffer buffer(channelCount, 4);
        REQUIRE(buffer.channelCount() == channelCount);
        REQUIRE(buffer.frameCount() == 4);

        for (int channel = 0; channel < channelCount; ++channel)
        {
            REQUIRE(buffer.channel(channel).size() == 4);

            for (const float sample : buffer.channel(channel))
                REQUIRE(sample == 0.0f);
        }

        for (int channel = 0; channel < channelCount; ++channel)
        {
            for (int frame = 0; frame < 4; ++frame)
                buffer.channel(channel)[frame] = static_cast<float>(channel * 4 + frame);
        }

        const AudioBuffer& source = buffer;

        for (int channel = 0; channel < channelCount; ++channel)
        {
            for (int frame = 0; frame < 4; ++frame)
                REQUIRE(source.channel(channel)[frame] == static_cast<float>(channel * 4 + frame));
        }

        // test operator as well
        for (int channel = 0; channel < channelCount; ++channel)
        {
            for (int frame = 0; frame < 4; ++frame)
                REQUIRE(source[channel][frame] == static_cast<float>(channel * 4 + frame));
        }
    }

    TEST_CASE("AudioBuffer: copies own their samples")
    {
        AudioBuffer source(2, 4);
        source.channel(0)[0] = 0.25f;
        source.channel(1)[3] = -0.5f;
        AudioBuffer copy(1, 1);

        SECTION("Copy construction")
        {
            AudioBuffer constructed(source);
            copy = std::move(constructed);
        }
        SECTION("Copy assignment")
        {
            copy = source;
        }

        REQUIRE(copy.channelCount() == 2);
        REQUIRE(copy.frameCount() == 4);
        REQUIRE(copy[0][0] == 0.25f);
        REQUIRE(copy[1][3] == -0.5f);

        source.channel(0)[0] = 1.0f;
        copy.channel(1)[3] = 1.0f;
        REQUIRE(copy.channel(0)[0] == 0.25f);
        REQUIRE(source.channel(1)[3] == -0.5f);
        REQUIRE(copy[0][0] == 0.25f);
        REQUIRE(source[1][3] == -0.5f);
    }

    TEST_CASE("AudioBuffer: moves preserve destination samples and allow source reuse")
    {
        AudioBuffer source(2, 4);
        source.channel(0)[0] = 0.25f;
        source.channel(1)[3] = -0.5f;
        AudioBuffer destination;

        SECTION("Move construction")
        {
            AudioBuffer constructed(std::move(source));
            destination = std::move(constructed);
        }
        SECTION("Move assignment")
        {
            destination = std::move(source);
        }

        REQUIRE(destination.channelCount() == 2);
        REQUIRE(destination.frameCount() == 4);
        REQUIRE(destination.channel(0)[0] == 0.25f);
        REQUIRE(destination.channel(1)[3] == -0.5f);
        REQUIRE(destination[0][0] == 0.25f);
        REQUIRE(destination[1][3] == -0.5f);


        source = AudioBuffer(1, 2);
        source.channel(0)[0] = 1.0f;
        REQUIRE(source.channelCount() == 1);
        REQUIRE(source.frameCount() == 2);
        REQUIRE(destination.channel(0)[0] == 0.25f);
        REQUIRE(destination[0][0] == 0.25f);
    }
} // namespace anasa