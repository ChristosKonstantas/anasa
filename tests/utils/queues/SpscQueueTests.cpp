#include <thread>
#include <vector>
#include <array>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "utils/queues/SpscQueue.hpp"
#include "audio/AudioBuffer.hpp"
#include "TestQueuePayload.hpp"
#include "TestQueueAllocator.hpp"

#ifdef enable_benchmarks

#include <iostream>
#include <memory>
#include <chrono>
#include <cmath>
#include <catch2/generators/catch_generators.hpp>
#include "benchmarks/Benchmark.hpp"
#include "utils/queues/SpscQueueOld1.hpp"
#include "utils/queues/SpscQueueOld2.hpp"
#include "audio/AudioTypes.hpp"

#endif // enable_benchmarks

struct NonCopyable // used to test if SpscQueue can accept a non-copyable / non-movable struct
{
    NonCopyable() = default;

    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;

    NonCopyable(NonCopyable&&) = delete;
    NonCopyable& operator=(NonCopyable&&) = delete;

    int value = 0;
};

TEST_CASE("SpscQueue: new queue is empty")
{
    constexpr size_t queueCapacity = 3;
    anasa::SpscQueue<int> queue(queueCapacity);

    REQUIRE(queue.isEmpty());
    REQUIRE(queue.capacity() == queueCapacity);

    REQUIRE_FALSE(queue.pop());
    REQUIRE(queue.front() == nullptr);
}

TEST_CASE("SpscQueue: push and pop one element")
{
    constexpr size_t queueCapacity = 3;
    anasa::SpscQueue<int> queue(queueCapacity);

    REQUIRE(queue.pushWith([](int& value){ value = 42;}));

    REQUIRE_FALSE(queue.isEmpty());

    int* value = queue.front();

    REQUIRE(value != nullptr);
    REQUIRE(*value == 42);

    REQUIRE(queue.pop());
    REQUIRE(queue.isEmpty());
}

TEST_CASE("SpscQueue: elements are popped in FIFO order")
{
    constexpr std::size_t queueCapacity = 4;
    anasa::SpscQueue<int> queue(queueCapacity);

    REQUIRE(queue.pushWith([](int& value){value = 10;}));

    REQUIRE(queue.pushWith([](int& value){value = 20;}));

    REQUIRE(queue.pushWith([](int& value){value = 30;}));

    int* value = queue.front();

    REQUIRE(value != nullptr);
    REQUIRE(*value == 10);
    REQUIRE(queue.pop());

    value = queue.front();

    REQUIRE(value != nullptr);
    REQUIRE(*value == 20);
    REQUIRE(queue.pop());

    value = queue.front();

    REQUIRE(value != nullptr);
    REQUIRE(*value == 30);
    REQUIRE(queue.pop());

    REQUIRE(queue.isEmpty());
}

TEST_CASE("SpscQueue: uses full physical capacity")
{
    constexpr std::size_t queueCapacity = 3;
    anasa::SpscQueue<int> queue(queueCapacity);

    REQUIRE(queue.capacity() == queueCapacity);

    REQUIRE(queue.pushWith([](int& value){value = 1;}));

    REQUIRE(queue.pushWith([](int& value){value = 2;}));

    REQUIRE(queue.pushWith([](int& value){value = 3;}));

    REQUIRE(queue.isFull());

    // All 3 physical slots are occupied.
    REQUIRE_FALSE(queue.pushWith([](int& value){value = 4;}));

    int* value = queue.front();

    REQUIRE(value != nullptr);
    REQUIRE(*value == 1);

    REQUIRE(queue.pop());

    // One slot became available.
    REQUIRE(queue.pushWith([](int& value){value = 4;}));
}

TEST_CASE("SpscQueue: front does not remove element")
{
    constexpr std::size_t queueCapacity = 3;
    anasa::SpscQueue<int> queue(queueCapacity);

    REQUIRE(queue.pushWith([](int& value){value = 1;}));

    int* first = queue.front();

    REQUIRE(first != nullptr);
    REQUIRE(*first == 1);

    // front() does not consume the element.
    REQUIRE_FALSE(queue.isEmpty());

    int* second = queue.front();

    REQUIRE(second != nullptr);
    REQUIRE(*second == 1);

    // It is literally the same queue-owned object.
    REQUIRE(second == first);

    REQUIRE(queue.pop());

    REQUIRE(queue.isEmpty());
    REQUIRE(queue.front() == nullptr);
}

TEST_CASE("SpscQueue: indices wrap around correctly")
{
    // Fill usable slots:
    //             
    // [1][2][3][4][5][6]
    //
    constexpr std::size_t queueCapacity = 6;
    anasa::SpscQueue<int> queue(queueCapacity);

    for (int i = 1; i <= queueCapacity; ++i)
        REQUIRE(queue.pushWith([i](int& value){value = i;}));

    REQUIRE(queue.isFull());

    // Free 5 slots.
    // 
    // [_][_][_][_][_][6]
    for (int expected = 1; expected <= queueCapacity - 1; ++expected)
    {
        int* value = queue.front();

        REQUIRE(value != nullptr);
        REQUIRE(*value == expected);

        REQUIRE(queue.pop());
    }

    REQUIRE_FALSE(queue.isFull());

    // Logical cursors continue increasing (monotonic) and physical storage positions wrap around.
    REQUIRE(queue.pushWith([](int& value){value = 6;}));
    REQUIRE(queue.pushWith([](int& value){value = 7;}));


    // Logical queue contents should now be:
    //
    // [6][7][_][_][_][6]
    //
    int* value = queue.front();

    REQUIRE(value != nullptr);
    REQUIRE(*value == 6);
    REQUIRE(queue.pop());

    value = queue.front();

    REQUIRE(value != nullptr);
    REQUIRE(*value == 6);
    REQUIRE(queue.pop());

    value = queue.front();

    REQUIRE(value != nullptr);
    REQUIRE(*value == 7);
    REQUIRE(queue.pop());

    // Queue is empty
    // [_][_][_][_][_][_]
    REQUIRE(queue.isEmpty());
    REQUIRE_FALSE(queue.isFull());
}

TEST_CASE("SpscQueue: full queue becomes writable after pop")
{
    constexpr std::size_t queueCapacity = 2;
    anasa::SpscQueue<int> queue(queueCapacity);

    REQUIRE(queue.pushWith([](int& value){value = 10;}));

    REQUIRE(queue.pushWith([](int& value){value = 20;}));

    REQUIRE(queue.isFull());

    REQUIRE_FALSE(queue.pushWith([](int& value){value = 30;}));

    int* value = queue.front();

    REQUIRE(value != nullptr);
    REQUIRE(*value == 10);

    REQUIRE(queue.pop());

    REQUIRE(queue.pushWith([](int& value){value = 30;}));

    value = queue.front();

    REQUIRE(value != nullptr);
    REQUIRE(*value == 20);
    REQUIRE(queue.pop());

    value = queue.front();

    REQUIRE(value != nullptr);
    REQUIRE(*value == 30);
    REQUIRE(queue.pop());

    REQUIRE(queue.isEmpty());
}

TEST_CASE("SpscQueue: repeated physical wrap-around preserves FIFO order")
{
    constexpr std::size_t queueCapacity = 3;
    anasa::SpscQueue<int> queue(queueCapacity);

    int expected = 0;

    for (int batch = 0; batch < 100; ++batch)
    {
        REQUIRE(queue.pushWith([expected](int& value){value = expected;}));

        REQUIRE(queue.pushWith([expected](int& value){value = expected + 1;}));

        REQUIRE(queue.pushWith([expected](int& value){value = expected + 2;}));

        for (int offset = 0; offset < 3; ++offset)
        {
            int* value = queue.front();

            REQUIRE(value != nullptr);
            REQUIRE(*value == expected + offset);

            REQUIRE(queue.pop());
        }

        expected += 3;

        REQUIRE(queue.isEmpty());
    }
}

TEST_CASE("SpscQueue: reports full state")
{
    constexpr std::size_t queueCapacity = 3;

    anasa::SpscQueue<int> queue(queueCapacity);

    REQUIRE_FALSE(queue.isFull());

    REQUIRE(queue.pushWith([](int& value){value = 1;}));

    REQUIRE_FALSE(queue.isFull());

    REQUIRE(queue.pushWith([](int& value){value = 2;}));

    REQUIRE_FALSE(queue.isFull());

    REQUIRE(queue.pushWith([](int& value){value = 3;}));

    REQUIRE(queue.isFull());

    REQUIRE(queue.pop());

    REQUIRE_FALSE(queue.isFull());
}

TEST_CASE("SpscQueue: producer and consumer can operate concurrently")
{
    constexpr std::size_t itemCount = 100000;
    constexpr std::size_t queueCapacity = 128;

    anasa::SpscQueue<int> queue(queueCapacity);

    std::vector<int> received;
    received.reserve(itemCount);

    std::thread producer([&]()
    {
        for (int i = 0; i < static_cast<int>(itemCount); ++i)
        {
            while (!queue.pushWith([i](int& value){value = i;}))
            {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&]()
    {
        for (std::size_t i = 0; i < itemCount; ++i)
        {
            int value = 0;

            while (true)
            {
                int* current = queue.front();

                if (current == nullptr)
                {
                    std::this_thread::yield();
                    continue;
                }

                // Read/copy the int before pop().
                value = *current;

                // current becomes invalid after this.
                REQUIRE(queue.pop());

                break;
            }

            received.push_back(value);
        }
    });

    producer.join();
    consumer.join();

    REQUIRE(received.size() == itemCount);

    for (std::size_t i = 0; i < itemCount; ++i)
    {
        REQUIRE(received[i] == static_cast<int>(i));
    }

    REQUIRE(queue.isEmpty());
}

TEST_CASE("SpscQueue transfers objects without copy or move")
{
    anasa::SpscQueue<NonCopyable> queue{4};

    REQUIRE(queue.pushWith([](NonCopyable& item){item.value = 42;}));

    NonCopyable* item = queue.front();

    REQUIRE(item != nullptr);
    REQUIRE(item->value == 42);

    REQUIRE(queue.pop());
}

TEST_CASE("SpscQueue: constructs configured slots and preserves their storage")
{
    const int numSlots = 3;
    const int size = 8;
    const int value = 44;

    anasa::SpscQueue<anasa::TestQueuePayload> queue(numSlots, size, value);
    REQUIRE(queue.isEmpty());
    std::array<const int*, numSlots> addresses{};

    for (int cycle = 0; cycle < numSlots; ++cycle)
    {
        for (int slot = 0; slot < numSlots; ++slot)
        {
            REQUIRE(queue.pushWith([&](anasa::TestQueuePayload& item)
            {
                REQUIRE(item.values.size() == size);

                if (cycle == 0)
                {
                    for (int storedValue : item.values)
                        REQUIRE(storedValue == value);

                    addresses[slot] = item.values.data();
                }
                else
                    REQUIRE(item.values.data() == addresses[slot]);

                item.values[0] = cycle * numSlots + slot;
            }));
        }

        bool called = false;
        REQUIRE_FALSE(queue.pushWith([&](anasa::TestQueuePayload&){ called = true; }));
        REQUIRE_FALSE(called);

        for (int slot = 0; slot < numSlots; ++slot)
        {
            const anasa::TestQueuePayload* item = queue.front();
            REQUIRE(item != nullptr);
            REQUIRE(item->values[0] == cycle * numSlots + slot);
            REQUIRE(queue.pop());
        }

        REQUIRE(queue.isEmpty());
        queue.reset();
    }

    REQUIRE_THROWS_AS(anasa::SpscQueue<anasa::TestQueuePayload>(0, 8, 42), std::invalid_argument);
}

TEST_CASE("SpscQueue: failed slot construction destroys objects and releases allocation")
{
    anasa::QueueConstructionState state;
    state.failOnAttempt = 3;
    using Allocator = anasa::TestQueueAllocator<anasa::TestQueuePayload>;
    using Queue = anasa::SpscQueue<anasa::TestQueuePayload, Allocator>;
    Allocator allocator(&state);

    REQUIRE_THROWS_AS(Queue(std::allocator_arg, allocator, 4, 8, 44, &state), std::runtime_error);

    REQUIRE(state.attempts == 3);
    REQUIRE(state.alive == 0);
    REQUIRE(state.destroyed == 2);
    REQUIRE(state.allocations == 1);
    REQUIRE(state.deallocations == 1);
}

TEST_CASE("SpscQueue: constructs AudioBuffer slots from dimensions")
{
    const int numSlots = 3;
    const int numChannels = 2;
    const int numSamples = 128;
    anasa::SpscQueue<anasa::AudioBuffer> queue(numSlots, numChannels, numSamples);
    REQUIRE(queue.isEmpty());

    for (int slot = 0; slot < numSlots; ++slot)
    {
        REQUIRE(queue.pushWith([&](anasa::AudioBuffer& buffer)
        {
            REQUIRE(buffer.channelCount() == numChannels);
            REQUIRE(buffer.frameCount() == numSamples);

            for (int channel = 0; channel < buffer.channelCount(); ++channel)
            {
                for (float& sample : buffer[channel])
                {
                    REQUIRE(sample == 0.0f);
                    sample = static_cast<float>(slot * 10 + channel);
                }
            }
        }));
    }

    for (int slot = 0; slot < numSlots; ++slot)
    {
        const anasa::AudioBuffer* buffer = queue.front();
        REQUIRE(buffer != nullptr);

        for (int channel = 0; channel < buffer->channelCount(); ++channel)
        {
            for (float sample : (*buffer)[channel])
                REQUIRE(sample == static_cast<float>(slot * 10 + channel));
        }

        REQUIRE(queue.pop());
    }

    REQUIRE(queue.isEmpty());
}

TEST_CASE("SpscQueue: copies AudioBuffer prototypes into independent slots")
{
    SECTION("Existing prototype")
    {
        const int numSlots = 3;
        const int numChannels = 2;
        const int numSamples = 128;
        anasa::AudioBuffer prototype(numChannels, numSamples);

        for (int channel = 0; channel < prototype.channelCount(); ++channel)
        {
            for (int frame = 0; frame < prototype.frameCount(); ++frame)
                prototype[channel][frame] = static_cast<float>(channel * 100 + frame);
        }

        anasa::SpscQueue<anasa::AudioBuffer> queue(numSlots, prototype);
        REQUIRE(queue.isEmpty());
        prototype[0][0] = -1.0f;
        std::array<const float*, numSlots> addresses{};

        for (int slot = 0; slot < numSlots; ++slot)
        {
            REQUIRE(queue.pushWith([&](anasa::AudioBuffer& buffer)
            {
                REQUIRE(buffer.channelCount() == numChannels);
                REQUIRE(buffer.frameCount() == numSamples);
                addresses[slot] = buffer[0].data();
                REQUIRE(addresses[slot] != prototype[0].data());

                for (int previous = 0; previous < slot; ++previous)
                    REQUIRE(addresses[slot] != addresses[previous]);

                for (int channel = 0; channel < buffer.channelCount(); ++channel)
                {
                    for (int frame = 0; frame < buffer.frameCount(); ++frame)
                        REQUIRE(buffer[channel][frame] == static_cast<float>(channel * 100 + frame));
                }

                buffer[0][0] = static_cast<float>(-2 - slot);
            }));
        }

        for (int slot = 0; slot < numSlots; ++slot)
        {
            const anasa::AudioBuffer* buffer = queue.front();
            REQUIRE(buffer != nullptr);
            REQUIRE((*buffer)[0][0] == static_cast<float>(-2 - slot));
            REQUIRE(queue.pop());
        }

        REQUIRE(prototype[0][0] == -1.0f);
        REQUIRE(queue.isEmpty());
    }

    SECTION("Temporary prototype")
    {
        int numSlots = 2;
        const int numChannels = 20;
        const int numSamples = 1024;
        anasa::SpscQueue<anasa::AudioBuffer> queue(numSlots, anasa::AudioBuffer(numChannels, numSamples));
        REQUIRE(queue.isEmpty());

        for (int slot = 0; slot < numSlots; ++slot)
        {
            REQUIRE(queue.pushWith([&](anasa::AudioBuffer& buffer)
            {
                REQUIRE(buffer.channelCount() == numChannels);
                REQUIRE(buffer.frameCount() == numSamples);
                for (int i = 0; i < numChannels; ++i)
                {
                    REQUIRE(buffer[i][0] == 0.0f);
                    REQUIRE(buffer[i][numSamples - 1] == 0.0f);
                }    
                buffer[numChannels - 1][numSamples - 1] = static_cast<float>(slot + 1);
            }));
        }

        for (int slot = 0; slot < numSlots; ++slot)
        {
            const anasa::AudioBuffer* buffer = queue.front();
            REQUIRE(buffer != nullptr);
            REQUIRE((*buffer)[numChannels - 1][numSamples - 1] == static_cast<float>(slot + 1));
            REQUIRE(queue.pop());
        }

        REQUIRE(queue.isEmpty());
    }
}

#ifdef enable_benchmarks
constexpr std::size_t QueueCapacity = 64;

void fillAudioBlock(anasa::AudioBlock& block, std::size_t index, int validFrames)
{
    block.generation = 1;
    block.firstFrame = static_cast<int>(index) * validFrames;
    block.frameCount = validFrames;

    for (int channel = 0; channel < block.samples.channelCount(); ++channel)
    {
        const auto samples = block.samples[channel];

        for (int frame = 0; frame < block.frameCount; ++frame)
            samples[frame] = 0.1f * (channel + 1);
    }
}

double consumeAudioBlock(const anasa::AudioBlock& block)
{
    double checksum = 0.0;

    for (int channel = 0; channel < block.samples.channelCount(); ++channel)
    {
        const auto samples = block.samples[channel];

        for (int frame = 0; frame < block.frameCount; ++frame)
            checksum += static_cast<double>(samples[frame]);
    }

    return checksum;
}

TEST_CASE("SpscQueue AudioBlock implementation comparison")
{
    const auto [validFrames, storageFrames] = GENERATE(Catch::Generators::table<int, int>
    (
        {
            {64, anasa::MAX_AUDIO_BLOCK_FRAMES},
            {128, anasa::MAX_AUDIO_BLOCK_FRAMES},
            {128, 128}
        }
    )
    );

    constexpr std::size_t framesPerRound = 64000000;
    constexpr std::size_t roundCount = 40;
    const std::size_t transfersPerRound = framesPerRound / static_cast<std::size_t>(validFrames);
    const std::size_t warmupTransfers = transfersPerRound / 10;

    const int numChannels = 2;

    CAPTURE(numChannels, validFrames, storageFrames);

    anasa::benchmarks::Benchmark benchmark(transfersPerRound, roundCount, warmupTransfers);

    anasa::SpscQueue<anasa::AudioBlock> currentQueue{QueueCapacity, numChannels, storageFrames };

    const anasa::benchmarks::BenchmarkResult current = benchmark.run([&](std::size_t i)
    {
        currentQueue.pushWith([&](anasa::AudioBlock& block)
        {
            fillAudioBlock(block, i, validFrames);
        });

        const anasa::AudioBlock* block = currentQueue.front();
        const double checksum = consumeAudioBlock(*block);

        currentQueue.pop();

        return checksum;
    });


    const anasa::AudioBlock initialBlock(numChannels, storageFrames);

    anasa::old1::SpscQueue<anasa::AudioBlock> old1Queue{QueueCapacity, initialBlock};

    anasa::AudioBlock old1Produced(numChannels, storageFrames);
    anasa::AudioBlock old1Head(numChannels, storageFrames);

    const anasa::benchmarks::BenchmarkResult old1 = benchmark.run([&](std::size_t i)
    {
        fillAudioBlock(old1Produced, i, validFrames);

        old1Queue.push(old1Produced);
        old1Queue.peek(old1Head);

        const double checksum = consumeAudioBlock(old1Head);

        old1Queue.pop(old1Head);

        return checksum;
    });


    std::unique_ptr<anasa::old2::SpscQueue<anasa::AudioBlock, static_cast<int>(QueueCapacity)>> old2Queue
        = std::make_unique<anasa::old2::SpscQueue<anasa::AudioBlock, static_cast<int>(QueueCapacity)>>(initialBlock);

    anasa::AudioBlock old2Produced(numChannels, storageFrames);
    anasa::AudioBlock old2Head(numChannels, storageFrames);

    const anasa::benchmarks::BenchmarkResult old2 = benchmark.run([&](std::size_t i)
    {
        fillAudioBlock(old2Produced, i, validFrames);

        old2Queue->push(old2Produced);
        old2Queue->peek(old2Head);

        const double checksum = consumeAudioBlock(old2Head);

        old2Queue->pop(old2Head);

        return checksum;
    });

    REQUIRE(std::isfinite(current.operationsPerSecond));
    REQUIRE(std::isfinite(current.nanosecondsPerOperation));
    REQUIRE(current.operationsPerSecond > 0.0);
    REQUIRE(current.nanosecondsPerOperation > 0.0);
    REQUIRE(current.checksum > 0.0);

    REQUIRE(std::isfinite(old1.operationsPerSecond));
    REQUIRE(std::isfinite(old1.nanosecondsPerOperation));
    REQUIRE(old1.operationsPerSecond > 0.0);
    REQUIRE(old1.nanosecondsPerOperation > 0.0);
    REQUIRE(old1.checksum > 0.0);

    REQUIRE(std::isfinite(old2.operationsPerSecond));
    REQUIRE(std::isfinite(old2.nanosecondsPerOperation));
    REQUIRE(old2.operationsPerSecond > 0.0);
    REQUIRE(old2.nanosecondsPerOperation > 0.0);
    REQUIRE(old2.checksum > 0.0);

    std::cout
        << "\n*--------------------------------* \n"
        << "|SpscQueue<AudioBlock> comparison| \n"
        << "*--------------------------------* \n"
        << "\nChannels: " << numChannels << "\n"
        << "Valid frames per channel: " << validFrames << "\n"
        << "Storage frames per channel: " << storageFrames << "\n"
        << "Execution: single-threaded round trip\n"
        << "\n---------------------------------- \n"
        << "\n(1)\n"
        << "\nCurrent - zero copy + pre-construction\n"
        << "Transfers: " << current.operationsPerSecond / 1000000.0f << " M transfers/sec\n"
        << "Time:      " << current.nanosecondsPerOperation << " ns/transfer\n"
        << "\n---------------------------------- \n"
        << "\n(2)\n"
        << "\nOld1 - allocator + copies + pre-construction \n"
        << "Transfers: " << old1.operationsPerSecond / 1000000.0f << " M transfers/sec\n"
        << "Time:      " << old1.nanosecondsPerOperation << " ns/transfer\n"
        << "\n---------------------------------- \n"
        << "\n(3)\n"
        << "\nOld2 - original std::array + copies\n"
        << "Transfers: " << old2.operationsPerSecond / 1000000.0f << " M transfers/sec\n"
        << "Time:      " << old2.nanosecondsPerOperation << " ns/transfer\n"
        << "\n---------------------------------- \n"
        << "\nSpeedup current vs Old1: "
        << old1.nanosecondsPerOperation / current.nanosecondsPerOperation << "x\n"
        << "Speedup current vs Old2: "
        << old2.nanosecondsPerOperation / current.nanosecondsPerOperation << "x\n"
        << "\n";

    REQUIRE(current.checksum == old1.checksum);
    REQUIRE(current.checksum == old2.checksum);
}

#endif //enable_benchmarks