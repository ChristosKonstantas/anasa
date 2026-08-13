#include <catch2/catch_test_macros.hpp>

#include "audio-pipeline/AudioSimulator.hpp"
#include "audio-pipeline/AudioConstants.hpp"
#include "utils/queues/SpscQueue.hpp"
#include "playback/PlaybackState.hpp"

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
        {
            AudioBlock block;

            block.generation = generation;
            block.firstFrame = blockIndex * settings.audioBlockFrames;
            block.frameCount = settings.audioBlockFrames;

            for (int i = 0; i < block.frameCount; ++i)
                block.samples[i] = 0.5f;
            
            REQUIRE(queue.push(block));
        }
    }

    TEST_CASE("AudioSimulator: paused playback produces no audio callbacks")
    {
        AudioSettings settings = makeTestAudioSettings();

        SharedState sharedState;

        SpscQueue<AudioBlock> readyAudioQueue(READY_AUDIO_QUEUE_SLOTS);
        
        AudioSimulator simulator(settings, sharedState, readyAudioQueue);

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

        SpscQueue<AudioBlock> readyAudioQueue(READY_AUDIO_QUEUE_SLOTS);

        sharedState.playing.store(true, std::memory_order_release);

        AudioSimulator simulator(settings, sharedState, readyAudioQueue);

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

        SharedState sharedState;

        SpscQueue<AudioBlock> readyAudioQueue(READY_AUDIO_QUEUE_SLOTS);

        constexpr int bufferedBlocks = 32;

        const int generation = sharedState.generation.load(std::memory_order_acquire);

        feedAudioQueue(readyAudioQueue, settings, bufferedBlocks, generation);

        sharedState.playing.store(true, std::memory_order_release);

        AudioSimulator simulator(settings, sharedState, readyAudioQueue);

        simulator.start();

        constexpr int callbacksToObserve = 4;

        REQUIRE(waitUntilFrameReaches(sharedState, callbacksToObserve * settings.audioBlockFrames));

        simulator.stop();

        REQUIRE(simulator.getCallbacksCount() >= callbacksToObserve);

        REQUIRE(simulator.getUnderrunsCount() == 0);

        // Non-zero samples must have been consumed.
        REQUIRE(simulator.getChecksum() > 0.0);
    }


    TEST_CASE("AudioSimulator: stop prevents further playback progress")
    {
        AudioSettings settings = makeTestAudioSettings();

        SharedState sharedState;

        SpscQueue<AudioBlock> readyAudioQueue(READY_AUDIO_QUEUE_SLOTS);

        constexpr int bufferedBlocks = 32;

        const int generation = sharedState.generation.load(std::memory_order_acquire);

        feedAudioQueue(readyAudioQueue, settings, bufferedBlocks, generation);

        sharedState.playing.store(true, std::memory_order_release);

        AudioSimulator simulator(settings, sharedState, readyAudioQueue);

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

        SpscQueue<AudioBlock> readyAudioQueue(READY_AUDIO_QUEUE_SLOTS);

        AudioSimulator simulator(settings, sharedState, readyAudioQueue);

        REQUIRE_NOTHROW(simulator.start());
        REQUIRE_NOTHROW(simulator.start());

        REQUIRE_NOTHROW(simulator.stop());
        REQUIRE_NOTHROW(simulator.stop());
    } 

} // namespace anasa