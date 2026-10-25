#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "scheduler/RenderCache.hpp"
#include "render/VersionTable.hpp"
#include "scheduler/TimelineAudioPublisher.hpp"

namespace anasa
{
    class TimelineAudioPublisherTestRig
    {
    public:
        static constexpr int QUEUE_CAPACITY = 2;
        static constexpr int CHUNK_COUNT = 2;
        static constexpr int BLOCK_FRAMES = 128;
        static constexpr int TOTAL_FRAMES = CHUNK_COUNT * CHUNK_FRAMES;

        explicit TimelineAudioPublisherTestRig(int channelCount = 1)
           : versions(CHUNK_COUNT),
             cache(channelCount, TOTAL_FRAMES, versions),
             queue(QUEUE_CAPACITY, channelCount, BLOCK_FRAMES + 1),
             publisher(BLOCK_FRAMES, channelCount, TOTAL_FRAMES, queue)
        {
            for (int chunk = 0; chunk < CHUNK_COUNT; ++chunk)
            {
                AudioBuffer chunkSamples(channelCount, CHUNK_FRAMES);

                for (int channel = 0; channel < channelCount; ++channel)
                {
                    const std::span<float> samples = chunkSamples[channel];

                    for (int frame = 0; frame < CHUNK_FRAMES; ++frame)
                        samples[frame] = static_cast<float>(channel * TOTAL_FRAMES + chunk * CHUNK_FRAMES + frame);
                }

                REQUIRE(cache.store(chunk, versions.get(chunk), chunkSamples));
            }
        }

        void requireBlock(int firstFrame, int generation)
        {
            const AudioBlock* block = queue.front();
            REQUIRE(block != nullptr);
            REQUIRE(block->generation == generation);
            REQUIRE(block->firstFrame == firstFrame);
            REQUIRE(block->frameCount == BLOCK_FRAMES);

            for (int channel = 0; channel < block->samples.channelCount(); ++channel)
            {
                const auto samples = block->samples[channel];

                for (int frame = 0; frame < BLOCK_FRAMES; ++frame)
                    REQUIRE(samples[frame] == static_cast<float>(channel * TOTAL_FRAMES + firstFrame + frame));

                REQUIRE(samples[BLOCK_FRAMES] == 0.0f);
            }

            REQUIRE(queue.pop());
        }

        VersionTable           versions;
        RenderCache            cache;
        SpscQueue<AudioBlock>  queue;
        TimelineAudioPublisher publisher;
    };

    TEST_CASE("TimelineAudioPublisher: rejects invalid configuration")
    {
        SpscQueue<AudioBlock> queue(2, 1, 128);
        int blockFrames = 128;
        int channelCount = 1;
        int totalFrames = CHUNK_FRAMES;

        SECTION("Zero block size") { blockFrames = 0; }
        SECTION("Negative block size") { blockFrames = -1; }
        SECTION("Excessive block size") { blockFrames = MAX_AUDIO_BLOCK_FRAMES + 1; }
        SECTION("Block does not divide a chunk") { blockFrames = 3; }
        SECTION("Zero channels") { channelCount = 0; }
        SECTION("Negative channels") { channelCount = -1; }
        SECTION("Empty timeline") { totalFrames = 0; }
        SECTION("Negative timeline") { totalFrames = -1; }
        SECTION("Partial final block") { totalFrames = CHUNK_FRAMES - 1; }

        REQUIRE_THROWS_AS(TimelineAudioPublisher(blockFrames, channelCount, totalFrames, queue), std::invalid_argument);
    }

    TEST_CASE("TimelineAudioPublisher: preserves every channel across a chunk boundary")
    {
        TimelineAudioPublisherTestRig rig(GENERATE(1, 2, 6, 16));
        const int firstFrame = CHUNK_FRAMES - rig.BLOCK_FRAMES;
        const int assignedGeneration = 5;
        rig.publisher.reset(firstFrame);
        rig.publisher.publish(firstFrame, assignedGeneration, rig.cache);

        REQUIRE(rig.publisher.readyLeadBlocks(firstFrame) == TimelineAudioPublisherTestRig::QUEUE_CAPACITY);
        REQUIRE_FALSE(rig.publisher.entireRemainderPublished());
        rig.requireBlock(firstFrame, assignedGeneration);
        rig.requireBlock(CHUNK_FRAMES, assignedGeneration);
        REQUIRE(rig.queue.isEmpty());
    }

    TEST_CASE("TimelineAudioPublisher: waits for unavailable content without skipping ahead")
    {
        TimelineAudioPublisherTestRig rig;
        const AudioBuffer* currentSamples = rig.cache.findCurrent(0);
        REQUIRE(currentSamples != nullptr);
        const AudioBuffer samples = *currentSamples;
        rig.versions.bump(0);

        const int firstFrame = 0;
        const int assignedGeneration = 1;

        rig.publisher.publish(firstFrame, assignedGeneration, rig.cache);
        REQUIRE(rig.queue.isEmpty());
        REQUIRE(rig.publisher.readyLeadBlocks(firstFrame) == 0);

        REQUIRE(rig.cache.store(0, rig.versions.get(0), samples));
        rig.publisher.publish(firstFrame, assignedGeneration, rig.cache);
        rig.requireBlock(firstFrame, assignedGeneration);
        rig.requireBlock(rig.BLOCK_FRAMES, assignedGeneration);
    }

    TEST_CASE("TimelineAudioPublisher: a full queue does not advance publication")
    {
        TimelineAudioPublisherTestRig rig;
        const int firstFrame = 0;
        const int assignedGeneration = 1;

        rig.publisher.publish(firstFrame, assignedGeneration, rig.cache);
        rig.requireBlock(firstFrame, assignedGeneration);

        rig.publisher.publish(rig.BLOCK_FRAMES, 1, rig.cache);
        REQUIRE(rig.publisher.readyLeadBlocks(rig.BLOCK_FRAMES) == TimelineAudioPublisherTestRig::QUEUE_CAPACITY);
        rig.requireBlock(rig.BLOCK_FRAMES, 1);
        rig.requireBlock(2 * rig.BLOCK_FRAMES, 1);
        REQUIRE(rig.queue.isEmpty());
    }

    TEST_CASE("TimelineAudioPublisher: skips unpublished audio already passed by the consumer")
    {
        TimelineAudioPublisherTestRig rig;
        const int currentFrame = 3 * rig.BLOCK_FRAMES;
        rig.publisher.publish(currentFrame, 1, rig.cache);

        REQUIRE(rig.publisher.readyLeadBlocks(currentFrame) == TimelineAudioPublisherTestRig::QUEUE_CAPACITY);
        REQUIRE(rig.publisher.readyLeadBlocks(rig.TOTAL_FRAMES) == 0);
        rig.requireBlock(currentFrame, 1);
        rig.requireBlock(currentFrame + rig.BLOCK_FRAMES, 1);
    }

    TEST_CASE("TimelineAudioPublisher: backward reset preserves queued blocks and publishes the new generation")
    {
        TimelineAudioPublisherTestRig rig;

        rig.publisher.reset(CHUNK_FRAMES);
        rig.publisher.publish(CHUNK_FRAMES, 1, rig.cache);

        rig.publisher.reset(0);
        rig.publisher.publish(0, 2, rig.cache);
        REQUIRE(rig.publisher.readyLeadBlocks(0) == 0);
        rig.requireBlock(CHUNK_FRAMES, 1);
        rig.requireBlock(CHUNK_FRAMES + rig.BLOCK_FRAMES, 1);

        rig.publisher.publish(0, 2, rig.cache);
        rig.requireBlock(0, 2);
        rig.requireBlock(rig.BLOCK_FRAMES, 2);
    }

    TEST_CASE("TimelineAudioPublisher: the remainder can be published before it is consumed")
    {
        TimelineAudioPublisherTestRig rig;
        const int lastBlock = rig.TOTAL_FRAMES - rig.BLOCK_FRAMES;
        rig.publisher.reset(lastBlock);
        rig.publisher.publish(lastBlock, 1, rig.cache);

        REQUIRE(rig.publisher.entireRemainderPublished());
        REQUIRE(rig.publisher.readyLeadBlocks(lastBlock) == 1);
        rig.requireBlock(lastBlock, 1);
        REQUIRE(rig.queue.isEmpty());

        rig.publisher.publish(rig.TOTAL_FRAMES, 1, rig.cache);
        REQUIRE(rig.publisher.readyLeadBlocks(rig.TOTAL_FRAMES) == 0);
        REQUIRE(rig.queue.isEmpty());

        rig.publisher.reset(0);
        REQUIRE_FALSE(rig.publisher.entireRemainderPublished());
    }
} // namespace anasa