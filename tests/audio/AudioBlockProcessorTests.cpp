#include <vector>
#include <array>
#include <span>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "audio/AudioBlockProcessor.hpp"
#include "playback/PlaybackProtocol.hpp"

#include "functions/Functions.hpp"


namespace anasa
{
    TEST_CASE("AudioBlockProcessor: rejects invalid internal block sizes")
    {
        SharedState state;
        SpscQueue<AudioBlock> queue(4);

        REQUIRE_THROWS_AS(AudioBlockProcessor(0, state, queue), std::invalid_argument);
        REQUIRE_THROWS_AS(AudioBlockProcessor(-1, state, queue), std::invalid_argument);
        REQUIRE_THROWS_AS(AudioBlockProcessor(MAX_AUDIO_BLOCK_FRAMES + 1, state, queue), std::invalid_argument);
    }

    TEST_CASE("AudioBlockProcessor: writes the requested output through its interface")
    {
        SharedState state;
        SpscQueue<AudioBlock> queue(4);
        AudioBlockProcessor processor(4, state, queue);
        IAudioBlockProcessor& callback = processor;
        std::array<float, 6> output;
        output.fill(functions::UNTOUCHED_SAMPLE);
                
        REQUIRE(functions::pushTestAudioBlock(queue, 1, 0, 4));
        state.playing.store(true, std::memory_order_release);

        float* channels[] = {output.data() + 1};
        const AudioProcessResult result = callback.processBlock({channels, 4});

        REQUIRE(callback.blockFrames() == 4);
        REQUIRE(result.playing);
        REQUIRE_FALSE(result.underrun);
        REQUIRE(output.front() == functions::UNTOUCHED_SAMPLE);
        REQUIRE(output.back() == functions::UNTOUCHED_SAMPLE);

        for (int frame = 1; frame <= 4; ++frame)
            REQUIRE(output[frame] == 0.5f);

        REQUIRE(queue.isEmpty());
        REQUIRE(state.nextUnconsumedFrame.load() == 4);
    }

    TEST_CASE("AudioBlockProcessor: pause and shutdown write silence without consuming")
    {
        SharedState state;
        SpscQueue<AudioBlock> queue(4);
        AudioBlockProcessor processor(4, state, queue);
        std::array<float, 4> output;
        output.fill(functions::UNTOUCHED_SAMPLE);
        float* channels[] = {output.data()};
        SECTION("Paused") {}
        SECTION("Stopped while playing")
        {
            state.playing.store(true, std::memory_order_release);
            state.stop.store(true, std::memory_order_release);
        }

        REQUIRE(functions::pushTestAudioBlock(queue, 1, 0, 4));
        const AudioProcessResult result = processor.processBlock({channels, 4});

        REQUIRE_FALSE(result.playing);
        REQUIRE_FALSE(result.underrun);
        REQUIRE_FALSE(queue.isEmpty());
        REQUIRE(state.nextUnconsumedFrame.load() == 0);

        for (const float sample : output)
            REQUIRE(sample == 0.0f);
    }

    TEST_CASE("AudioBlockProcessor: missing or mismatched blocks produce silence and advance")
    {
        SharedState state;
        SpscQueue<AudioBlock> queue(4);
        AudioBlockProcessor processor(4, state, queue);
        std::array<float, 4> output;
        output.fill(functions::UNTOUCHED_SAMPLE);
        state.playing.store(true, std::memory_order_release);
        float* channels[] = {output.data()};
        SECTION("Empty queue") {}
        SECTION("Future frame") { REQUIRE(functions::pushTestAudioBlock(queue, 1, 4, 4)); }
        SECTION("Future generation") { REQUIRE(functions::pushTestAudioBlock(queue, 2, 0, 4)); }
        SECTION("Wrong size") { REQUIRE(functions::pushTestAudioBlock(queue, 1, 0, 3)); }

        const bool hadBlock = !queue.isEmpty();
        const AudioProcessResult result = processor.processBlock({channels, 4});

        REQUIRE(result.playing);
        REQUIRE(result.underrun);
        REQUIRE(state.nextUnconsumedFrame.load() == 4);
        REQUIRE(queue.isEmpty() == !hadBlock);

        for (const float sample : output)
            REQUIRE(sample == 0.0f);
    }

    TEST_CASE("AudioBlockProcessor: keeps a future block and discards audio arriving too late")
    {
        SharedState state;
        SpscQueue<AudioBlock> queue(4);
        AudioBlockProcessor processor(4, state, queue);
        std::array<float, 4> output{};
        float* channels[] = {output.data()};
        state.playing.store(true, std::memory_order_release);

        REQUIRE(processor.processBlock({channels, 4}).underrun);
        REQUIRE(functions::pushTestAudioBlock(queue, 1, 0, 4));
        REQUIRE(functions::pushTestAudioBlock(queue, 1, 8, 4, 0.25f));

        REQUIRE(processor.processBlock({channels, 4}).underrun);
        REQUIRE(queue.front() != nullptr);
        REQUIRE(queue.front()->firstFrame == 8);
        REQUIRE_FALSE(processor.processBlock({channels, 4}).underrun);
        REQUIRE(state.nextUnconsumedFrame.load() == 12);
        REQUIRE(queue.isEmpty());

        for (const float sample : output)
            REQUIRE(sample == 0.25f);
    }

    TEST_CASE("AudioBlockProcessor: generation reset acknowledges the target while paused")
    {
        SharedState state;
        PlaybackProtocol protocol(16, state);
        SpscQueue<AudioBlock> queue(4);
        AudioBlockProcessor processor(4, state, queue);
        std::array<float, 4> output{};
        float* channels[] = {output.data()};
        state.playing.store(true, std::memory_order_release);

        REQUIRE(functions::pushTestAudioBlock(queue, 1, 0, 4));
        REQUIRE_FALSE(processor.processBlock({channels, 4}).underrun);

        protocol.beginGeneration(8, true);
        REQUIRE(state.audioCursorGeneration.load() == 1);
        REQUIRE(protocol.currentFrame() == 8);
        REQUIRE(functions::pushTestAudioBlock(queue, 1, 4, 4));
        REQUIRE(functions::pushTestAudioBlock(queue, 2, 4, 4));
        REQUIRE(functions::pushTestAudioBlock(queue, 2, 8, 4, 0.75f));

        REQUIRE_FALSE(processor.processBlock({channels, 4}).playing);
        REQUIRE(state.audioCursorGeneration.load() == 2);
        REQUIRE(state.nextUnconsumedFrame.load() == 8);
        REQUIRE(protocol.currentFrame() == 8);
        REQUIRE(queue.front() != nullptr);
        REQUIRE(queue.front()->generation == 2);
        REQUIRE(queue.front()->firstFrame == 8);

        for (const float sample : output)
            REQUIRE(sample == 0.0f);

        state.playing.store(true, std::memory_order_release);
        REQUIRE_FALSE(processor.processBlock({channels, 4}).underrun);
        REQUIRE(state.nextUnconsumedFrame.load() == 12);
        REQUIRE(protocol.currentFrame() == 12);
        REQUIRE(queue.isEmpty());

        for (const float sample : output)
            REQUIRE(sample == 0.75f);
    }
    
    TEST_CASE("AudioBlockProcessor: copies mono to every output and advances once")
    {
        const int channelCount = GENERATE(1, 2, 6, 16);
        SharedState state;
        SpscQueue<AudioBlock> queue(4);
        AudioBlockProcessor processor(4, state, queue);
        std::vector<std::array<float, 6>> output(channelCount);
        std::vector<float*> channels(channelCount);

        for (int channel = 0; channel < channelCount; ++channel)
        {
            output[channel].fill(functions::UNTOUCHED_SAMPLE);
            channels[channel] = output[channel].data() + 1;
        }

        REQUIRE(queue.pushWith([](AudioBlock& block)
        {
            block.generation = 1;
            block.firstFrame = 0;
            block.frameCount = 4;

            for (int frame = 0; frame < 4; ++frame)
                block.samples[0][frame] = 0.125f * (frame + 1);
        }));
        state.playing.store(true, std::memory_order_release);

        const AudioProcessResult result = processor.processBlock({channels, 4});
        REQUIRE(result.playing);
        REQUIRE_FALSE(result.underrun);
        REQUIRE(queue.isEmpty());
        REQUIRE(state.nextUnconsumedFrame.load() == 4);

        for (const std::array<float, 6>& channel : output)
        {
            REQUIRE(channel.front() == functions::UNTOUCHED_SAMPLE);
            REQUIRE(channel.back() == functions::UNTOUCHED_SAMPLE);

            for (int frame = 1; frame <= 4; ++frame)
                REQUIRE(channel[frame] == 0.125f * frame);
        }
    }

    TEST_CASE("AudioBlockProcessor: handles disabled channels and silences every active output")
    {
        SharedState state;
        SpscQueue<AudioBlock> queue(4);
        AudioBlockProcessor processor(4, state, queue);
        std::array<float, 4> left;
        std::array<float, 4> right;
        left.fill(functions::UNTOUCHED_SAMPLE);
        right.fill(functions::UNTOUCHED_SAMPLE);
        float* channels[] = {left.data(), nullptr, right.data()};
        bool advances = false;
        bool underrun = false;
        float expected = 0.0f;

        SECTION("Paused") {}
        SECTION("Stopped")
        {
            state.playing.store(true);
            state.stop.store(true);
        }
        SECTION("Underrun")
        {
            state.playing.store(true);
            advances = true;
            underrun = true;
        }
        SECTION("Ready audio")
        {
            state.playing.store(true);
            REQUIRE(functions::pushTestAudioBlock(queue, 1, 0, 4));
            advances = true;
            expected = 0.5f;
        }

        const AudioProcessResult result = processor.processBlock({channels, 4});
        REQUIRE(result.playing == advances);
        REQUIRE(result.underrun == underrun);
        REQUIRE(state.nextUnconsumedFrame.load() == (advances ? 4 : 0));

        for (int frame = 0; frame < 4; ++frame)
        {
            REQUIRE(left[frame] == expected);
            REQUIRE(right[frame] == expected);
        }
    }

    TEST_CASE("AudioBlockProcessor: empty output does not consume or advance")
    {
        SharedState state;
        SpscQueue<AudioBlock> queue(4);
        AudioBlockProcessor processor(4, state, queue);
        std::array<float, 4> samples;
        samples.fill(functions::UNTOUCHED_SAMPLE);
        float* active[] = {samples.data()};
        float* disabled[] = {nullptr, nullptr};
        AudioOutputBuffer output{active, 4};

        SECTION("Zero frames") { output.frameCount = 0; }
        SECTION("Zero channels") { output.channels = {}; }
        SECTION("All channels disabled") { output.channels = disabled; }

        REQUIRE(functions::pushTestAudioBlock(queue, 1, 0, 4));
        state.playing.store(true);
        const AudioProcessResult result = processor.processBlock(output);

        REQUIRE_FALSE(result.playing);
        REQUIRE_FALSE(result.underrun);
        REQUIRE_FALSE(queue.isEmpty());
        REQUIRE(state.nextUnconsumedFrame.load() == 0);

        for (const float sample : samples)
            REQUIRE(sample == functions::UNTOUCHED_SAMPLE);
    }
} // namespace anasa