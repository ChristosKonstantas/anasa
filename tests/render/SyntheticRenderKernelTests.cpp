#include <array>
#include <cmath>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "render/kernels/SyntheticRenderKernel.hpp"

namespace anasa
{
    TEST_CASE("SyntheticRenderKernel: rejects invalid settings")
    {
        REQUIRE_THROWS_AS(SyntheticRenderKernel(0, 4), std::invalid_argument);
        REQUIRE_THROWS_AS(SyntheticRenderKernel(-48000, 4), std::invalid_argument);
        REQUIRE_THROWS_AS(SyntheticRenderKernel(48000, -1), std::invalid_argument);
    }

    TEST_CASE("SyntheticRenderKernel: preserves sample output")
    {
        struct SampleCase
        {
            int   sampleRate;
            int   workIterations;
            int   globalFrame;
            int   version;
            float expected;
        };

        const std::array<SampleCase, 6> cases
        {{
            {48000,   0,    0,  1,  0.0f},
            {48000,   0,  255,  1, -0.0511539057f},
            {48000,   4,  256,  7, -0.0660530329f},
            {44100,   4, 2048, 13,  0.187635064f},
            {48000, 300, 8191, 12, -0.0571736507f},
            {44100, 300, 4096, 14,  0.0746598989f}
        }};

        for (const SampleCase& sample : cases)
        {
            CAPTURE(sample.sampleRate, sample.workIterations, sample.globalFrame, sample.version);
            SyntheticRenderKernel kernel(sample.sampleRate, sample.workIterations);

            const float actual = kernel.renderSample(0, sample.globalFrame, sample.version);
            REQUIRE(std::abs(actual - sample.expected) <= 0.000001f);
        }
    }

    TEST_CASE("SyntheticRenderKernel: renders deterministic distinct channels")
    {
        SyntheticRenderKernel kernel(48000, 0);
        const float mono = kernel.renderSample(0, 255, 7);

        for (int channel = 1; channel < 16; ++channel)
        {
            const float sample = kernel.renderSample(channel, 255, 7);
            REQUIRE(std::isfinite(sample));
            REQUIRE(std::abs(sample) <= 1.0f);
            REQUIRE(sample != mono);
            REQUIRE(sample == kernel.renderSample(channel, 255, 7));
        }

        REQUIRE_THROWS_AS(kernel.renderSample(-1, 255, 7), std::out_of_range);
    }

} // namespace anasa