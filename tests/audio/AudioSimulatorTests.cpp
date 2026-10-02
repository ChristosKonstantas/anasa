#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "audio/simulator/AudioSimulator.hpp"
#include "audio/AudioConstants.hpp"
#include "utils/queues/SpscQueue.hpp"
#include "playback/PlaybackState.hpp"
#include "audio/AudioBlockProcessor.hpp"
#include "functions/Functions.hpp"
#include "TestAudioBlockProcessor.hpp"
#include <stdexcept>
#include <chrono>
#include <thread>

namespace anasa
{
    using namespace std::chrono_literals;

    AudioSettings makeTestAudioSettings(int sampleRate = 48000, int audioBlockFrames = 128)
    {
        AudioSettings settings;

        settings.sampleRate = sampleRate;
        settings.audioBlockFrames = audioBlockFrames;

        return settings;
    }

    bool waitUntilFrameReaches(SharedState& state, int targetFrame, std::chrono::milliseconds timeout = 250ms)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        while (std::chrono::steady_clock::now() < deadline)
        {
            if (state.nextUnconsumedFrame.load(std::memory_order_acquire) >= targetFrame)
            {
                return true;
            }

            std::this_thread::sleep_for(1ms);
        }

        return false;
    }

    void feedAudioQueue(SpscQueue<AudioBlock>& queue, const AudioSettings& settings, int blockCount, int generation = 1)
    {
        for (int blockIndex = 0; blockIndex < blockCount; ++blockIndex)
            REQUIRE(functions::pushTestAudioBlock(queue, generation, blockIndex * settings.audioBlockFrames, settings.audioBlockFrames));
    }

    TEST_CASE("AudioSimulator: paused callbacks do not advance playback")
    {
        AudioSettings settings = makeTestAudioSettings();

        SharedState sharedState;

        SpscQueue<AudioBlock> readyAudioQueue(READY_AUDIO_QUEUE_SLOTS, settings.channelCount, settings.audioBlockFrames);
        
        AudioBlockProcessor processor(settings.audioBlockFrames, sharedState, readyAudioQueue);
        AudioSimulator simulator(settings, sharedState.stop, processor);

        // SharedState starts with playing == false.
        simulator.start();

        std::this_thread::sleep_for(20ms);

        simulator.stop();

        REQUIRE(simulator.getCallbacksCount() == 0);
        REQUIRE(simulator.getUnderrunsCount() == 0);

        REQUIRE(sharedState.nextUnconsumedFrame.load(std::memory_order_acquire) == 0);
    }


    TEST_CASE("AudioSimulator: empty queue causes underruns during playback")
    {
        AudioSettings settings = makeTestAudioSettings();

        SharedState sharedState;

        SpscQueue<AudioBlock> readyAudioQueue(READY_AUDIO_QUEUE_SLOTS, settings.channelCount, settings.audioBlockFrames);

        sharedState.playing.store(true, std::memory_order_release);

        AudioBlockProcessor processor(settings.audioBlockFrames, sharedState, readyAudioQueue);
        AudioSimulator simulator(settings, sharedState.stop, processor);

        simulator.start();

        constexpr int expectedCallbacks = 4;

        REQUIRE(waitUntilFrameReaches(sharedState, expectedCallbacks * settings.audioBlockFrames));

        simulator.stop();

        REQUIRE(simulator.getCallbacksCount() >= expectedCallbacks);

        // No blocks were ever provided.
        REQUIRE(simulator.getUnderrunsCount() == simulator.getCallbacksCount());

        REQUIRE(simulator.getChecksum() == 0.0);
    }


    TEST_CASE("AudioSimulator: consumes ready audio blocks without underrun")
    {
        AudioSettings settings = makeTestAudioSettings();
        settings.channelCount = GENERATE(1, 2, 6, 16);
        SharedState sharedState;

        SpscQueue<AudioBlock> readyAudioQueue(READY_AUDIO_QUEUE_SLOTS, settings.channelCount, settings.audioBlockFrames);

        constexpr int bufferedBlocks = 32;

        const int generation = sharedState.generation.load(std::memory_order_acquire);

        feedAudioQueue(readyAudioQueue, settings, bufferedBlocks, generation);

        sharedState.playing.store(true, std::memory_order_release);

        AudioBlockProcessor processor(settings.audioBlockFrames, sharedState, readyAudioQueue);
        AudioSimulator simulator(settings, sharedState.stop, processor);

        simulator.start();

        constexpr int callbacksToObserve = 4;

        REQUIRE(waitUntilFrameReaches(sharedState, callbacksToObserve * settings.audioBlockFrames));

        simulator.stop();

        REQUIRE(simulator.getCallbacksCount() >= callbacksToObserve);

        REQUIRE(simulator.getUnderrunsCount() == 0);

        // Non-zero samples must have been consumed.
        REQUIRE(simulator.getChecksum() == simulator.getCallbacksCount() * settings.audioBlockFrames * settings.channelCount * 0.25);
    }


    TEST_CASE("AudioSimulator: stop prevents further playback progress")
    {
        AudioSettings settings = makeTestAudioSettings();

        SharedState sharedState;

        SpscQueue<AudioBlock> readyAudioQueue(READY_AUDIO_QUEUE_SLOTS, settings.channelCount, settings.audioBlockFrames);

        constexpr int bufferedBlocks = 32;

        const int generation = sharedState.generation.load(std::memory_order_acquire);

        feedAudioQueue(readyAudioQueue, settings, bufferedBlocks, generation);

        sharedState.playing.store(true, std::memory_order_release);

        AudioBlockProcessor processor(settings.audioBlockFrames, sharedState, readyAudioQueue);
        AudioSimulator simulator(settings, sharedState.stop, processor);

        simulator.start();

        REQUIRE(waitUntilFrameReaches(sharedState, 2 * settings.audioBlockFrames));

        simulator.stop();

        const int frameAfterStop = sharedState.nextUnconsumedFrame.load(std::memory_order_acquire);

        std::this_thread::sleep_for(20ms);

        REQUIRE(sharedState.nextUnconsumedFrame.load(std::memory_order_acquire) == frameAfterStop);
    }


    TEST_CASE("AudioSimulator: start and stop are idempotent")
    {
        AudioSettings settings = makeTestAudioSettings();

        SharedState sharedState;

        SpscQueue<AudioBlock> readyAudioQueue(READY_AUDIO_QUEUE_SLOTS, settings.channelCount, settings.audioBlockFrames);

        AudioBlockProcessor processor(settings.audioBlockFrames, sharedState, readyAudioQueue);
        AudioSimulator simulator(settings, sharedState.stop, processor);

        REQUIRE_NOTHROW(simulator.start());
        REQUIRE_NOTHROW(simulator.start());

        REQUIRE_NOTHROW(simulator.stop());
        REQUIRE_NOTHROW(simulator.stop());
    }

    TEST_CASE("AudioSimulator: drives an injected processor without playback state or a queue")
    {
        TestAudioBlockProcessor processor;
        std::atomic<bool> engineStop{false};
        
        const int channelCount = GENERATE(1, 2, 6, 16);
        AudioSettings settings = makeTestAudioSettings();
        settings.channelCount = channelCount;
        AudioSimulator simulator(settings, engineStop, processor);

        simulator.start();
        const bool processed = functions::waitUntil([&]
        {
            return processor.calls.load(std::memory_order_acquire) >= 3;
        });
        simulator.stop();

        REQUIRE(processed);
        REQUIRE(simulator.getCallbacksCount() == processor.calls.load());
        REQUIRE(simulator.getUnderrunsCount() == 0);
        REQUIRE(simulator.getChecksum() == simulator.getCallbacksCount() * 128 * 0.25 * channelCount);
    }

    TEST_CASE("AudioSimulator: rejects unsupported or mismatched settings")
    {
        TestAudioBlockProcessor processor;
        std::atomic<bool> engineStop{false};
        AudioSettings settings = makeTestAudioSettings();

        SECTION("Zero sample rate") { settings.sampleRate = 0; }
        SECTION("Negative sample rate") { settings.sampleRate = -1; }
        SECTION("Zero callback size") { settings.audioBlockFrames = 0; }
        SECTION("Oversized callback") { settings.audioBlockFrames = MAX_AUDIO_BLOCK_FRAMES + 1; }
        SECTION("Mismatched block sizes") { settings.audioBlockFrames = 64; }
        SECTION("Zero channels") { settings.channelCount = 0; }
        SECTION("Negative channels") { settings.channelCount = -1; }
        REQUIRE_THROWS_AS(AudioSimulator(settings, engineStop, processor), std::invalid_argument);
    }

} // namespace anasa