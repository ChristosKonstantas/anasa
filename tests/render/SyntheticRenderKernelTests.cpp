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

    TEST_CASE("SyntheticRenderKernel: preserves R1a sample output")
    {
        struct SampleCase
        {
            int   sampleRate;
            int   workIterations;
            int   globalFrame;
            int   version;
            float expected;
        };

        // Reference samples recorded from R1a commit 1d94e85.
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

            const float actual = kernel.renderSample(sample.globalFrame, sample.version);
            REQUIRE(std::abs(actual - sample.expected) <= 0.000001f);
        }
    }
} // namespace anasa