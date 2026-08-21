#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "utils/queues/SpscQueue.hpp"

#ifdef enable_benchmarks

#include <iostream>
#include <memory>
#include <chrono>

#include "benchmarks/Benchmark.hpp"
#include "utils/queues/SpscQueueOld1.hpp"
#include "utils/queues/SpscQueueOld2.hpp"
#include "audio-pipeline/AudioTypes.hpp"

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

constexpr std::size_t QueueCapacity = 64;
constexpr int AudioBlockFrames = 64;

void fillAudioBlock(anasa::AudioBlock& block, std::size_t index)
{
    block.generation = 1;
    block.firstFrame = static_cast<int>(index) * AudioBlockFrames;
    block.frameCount = AudioBlockFrames;

    for (int i = 0; i < block.frameCount; ++i)
        block.samples[i] = 0.1f;
}

double consumeAudioBlock(const anasa::AudioBlock& block)
{
    double checksum = 0.0;

    for (int i = 0; i < block.frameCount; ++i)
        checksum += static_cast<double>(block.samples[i]);

    return checksum;
}

#ifdef enable_benchmarks
TEST_CASE("SpscQueue AudioBlock implementation comparison")
{
    constexpr std::size_t transfersPerRound = 1000000;
    constexpr std::size_t roundCount = 40;
    constexpr std::size_t warmupTransfers = 100000;

    anasa::benchmarks::Benchmark benchmark(transfersPerRound, roundCount, warmupTransfers);

    anasa::SpscQueue<anasa::AudioBlock> currentQueue{QueueCapacity};

    const anasa::benchmarks::BenchmarkResult current = benchmark.run([&](std::size_t i)
    {
        currentQueue.pushWith([&](anasa::AudioBlock& block)
        {
            fillAudioBlock(block, i);
        });

        const anasa::AudioBlock* block = currentQueue.front();
        const double checksum = consumeAudioBlock(*block);

        currentQueue.pop();

        return checksum;
    });


    anasa::old1::SpscQueue<anasa::AudioBlock> old1Queue{QueueCapacity};

    anasa::AudioBlock old1Produced{};
    anasa::AudioBlock old1Head{};

    const anasa::benchmarks::BenchmarkResult old1 = benchmark.run([&](std::size_t i)
    {
        fillAudioBlock(old1Produced, i);

        old1Queue.push(old1Produced);
        old1Queue.peek(old1Head);

        const double checksum = consumeAudioBlock(old1Head);

        old1Queue.pop(old1Head);

        return checksum;
    });


    std::unique_ptr<anasa::old2::SpscQueue<anasa::AudioBlock, static_cast<int>(QueueCapacity)>> old2Queue
     = std::make_unique<anasa::old2::SpscQueue<anasa::AudioBlock, static_cast<int>(QueueCapacity)>>();

    anasa::AudioBlock old2Produced{};
    anasa::AudioBlock old2Head{};

    const anasa::benchmarks::BenchmarkResult old2 = benchmark.run([&](std::size_t i)
    {
        fillAudioBlock(old2Produced, i);

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