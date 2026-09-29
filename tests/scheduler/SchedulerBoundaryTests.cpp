#include <limits>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "execution/Executor.hpp"
#include "functions/Functions.hpp"
#include "render/Renderer.hpp"
#include "render/kernels/SyntheticRenderKernel.hpp"
#include "scheduler/Scheduler.hpp"

namespace anasa
{
    TEST_CASE("Scheduler: rejects invalid frame configuration without overflowing")
    {
        AudioSettings audioSettings;
        int totalFrames = CHUNK_FRAMES;

        SECTION("Negative sample rate")
        {
            audioSettings.sampleRate = std::numeric_limits<int>::min();
        }
        SECTION("Zero channels") { audioSettings.channelCount = 0; }
        SECTION("Negative channels") { audioSettings.channelCount = -1; }
        SECTION("Large mismatched timeline")
        {
            audioSettings.audioBlockFrames = 1;
            totalFrames = std::numeric_limits<int>::max();
        }

        SharedState state;
        VersionTable versions(1);
        SpscQueue<AudioBlock> queue(64);
        SyntheticRenderKernel kernel(48000, 0);
        Renderer renderer(kernel, versions);
        Executor executor(ExecutorSettings{}, renderer);

        REQUIRE_THROWS_AS(Scheduler(SchedulerSettings{}, audioSettings, RenderSettings{}, totalFrames, state, versions, executor, queue), std::invalid_argument);
    }

    TEST_CASE("Scheduler: clamps large settings before frame arithmetic")
    {
        AudioSettings audioSettings;
        RenderSettings renderSettings;
        renderSettings.contextFrames = 0;
        renderSettings.workIterations = 0;
        SchedulerSettings schedulerSettings;
        bool affectsBothChunks = false;

        SECTION("Large edit context")
        {
            renderSettings.contextFrames = std::numeric_limits<int>::max();
            affectsBothChunks = true;
        }
        SECTION("Large initial viewport")
        {
            audioSettings.sampleRate = std::numeric_limits<int>::max();
        }
        SECTION("Large urgent window")
        {
            schedulerSettings.urgentChunks = std::numeric_limits<int>::max();
        }

        SharedState state;
        VersionTable versions(2);
        SpscQueue<AudioBlock> audioQueue(64);
        SyntheticRenderKernel kernel(audioSettings.sampleRate, renderSettings.workIterations);
        Renderer renderer(kernel, versions);
        Executor executor(ExecutorSettings{}, renderer);
        Scheduler scheduler(schedulerSettings, audioSettings, renderSettings, 2 * CHUNK_FRAMES, state, versions, executor, audioQueue);
        
        executor.start();
        scheduler.start();

        REQUIRE(scheduler.post({CommandType::Seek, CHUNK_FRAMES, 0}));
        REQUIRE(scheduler.post({CommandType::Play, 0, 0}));
        REQUIRE(scheduler.post({CommandType::Edit, CHUNK_FRAMES + 128, CHUNK_FRAMES + 128}));

        functions::BlockHeader block;
        REQUIRE(functions::waitAndPopFirstBlockOfGeneration(audioQueue, 3, block));
        REQUIRE(block.firstFrame == CHUNK_FRAMES);
        REQUIRE(block.frameCount == audioSettings.audioBlockFrames);
        REQUIRE(versions.get(0) == (affectsBothChunks ? 2 : 1));
        REQUIRE(versions.get(1) == 2);
    }
} // namespace anasa