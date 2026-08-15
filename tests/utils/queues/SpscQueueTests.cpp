#include <catch2/catch_test_macros.hpp>
#include "utils/queues/SpscQueue.hpp"

#include <thread>
#include <vector>
#include <chrono>
#include <iostream>

TEST_CASE("SpscQueue: new queue is empty")
{
    constexpr size_t queueCapacity = 3;
    anasa::SpscQueue<int> queue(queueCapacity);

    REQUIRE(queue.isEmpty());
    REQUIRE(queue.capacity() == queueCapacity);

    int value = 0;
    REQUIRE_FALSE(queue.pop(value));
    REQUIRE_FALSE(queue.peek(value));
}


TEST_CASE("SpscQueue: push and pop one element")
{
    constexpr size_t queueCapacity = 3;
    anasa::SpscQueue<int> queue(queueCapacity);

    REQUIRE(queue.push(42));
    REQUIRE_FALSE(queue.isEmpty());

    int value = 0;
    REQUIRE(queue.pop(value));

    REQUIRE(value == 42);
    REQUIRE(queue.isEmpty());
}


TEST_CASE("SpscQueue: elements are popped in FIFO order")
{
    constexpr size_t queueCapacity = 4;
    anasa::SpscQueue<int> queue(queueCapacity);

    REQUIRE(queue.push(10));
    REQUIRE(queue.push(20));
    REQUIRE(queue.push(30));

    int value = 0;

    REQUIRE(queue.pop(value));
    REQUIRE(value == 10);

    REQUIRE(queue.pop(value));
    REQUIRE(value == 20);

    REQUIRE(queue.pop(value));
    REQUIRE(value == 30);

    REQUIRE(queue.isEmpty());
}


TEST_CASE("SpscQueue: one slot is reserved")
{
    // Physical capacity = 3
    // Usable capacity   = 3
    constexpr size_t queueCapacity = 3;
    anasa::SpscQueue<int> queue(queueCapacity);

    REQUIRE(queue.capacity() == queueCapacity);

    REQUIRE(queue.push(1));
    REQUIRE(queue.push(2));
    REQUIRE(queue.push(3));

    // Queue is full now...
    REQUIRE_FALSE(queue.push(4));

    int value = 0;

    REQUIRE(queue.pop(value));
    REQUIRE(value == 1);

    // Removing one element allows push again.
    REQUIRE(queue.push(4));
}


TEST_CASE("SpscQueue: peek does not remove element")
{
    constexpr size_t queueCapacity = 3;
    anasa::SpscQueue<int> queue(queueCapacity);

    REQUIRE(queue.push(1));

    int firstPeek = 0;
    int secondPeek = 0;
    int popped = 0;

    REQUIRE(queue.peek(firstPeek));
    REQUIRE(firstPeek == 1);

    // The element should still be there.
    REQUIRE_FALSE(queue.isEmpty());

    REQUIRE(queue.peek(secondPeek));
    REQUIRE(secondPeek == 1);

    // pop() must still return the same element.
    REQUIRE(queue.pop(popped));
    REQUIRE(popped == 1);

    REQUIRE(queue.isEmpty());
}


TEST_CASE("SpscQueue: indices wrap around correctly")
{
    constexpr size_t queueCapacity = 6;
    anasa::SpscQueue<int> queue(queueCapacity);

    // Fill usable slots:
    // w/r              
    // [1][2][3][4][5][6]
    //
    REQUIRE(queue.push(1));
    REQUIRE(queue.push(2));
    REQUIRE(queue.push(3));
    REQUIRE(queue.push(4));
    REQUIRE(queue.push(5));
    REQUIRE(queue.push(6));
    REQUIRE(queue.isFull());

    int value = 0;

    // Free 5 slots.
    //  w              r
    // [_][_][_][_][_][6]
    REQUIRE(queue.pop(value));
    REQUIRE(value == 1);

    REQUIRE(queue.pop(value));
    REQUIRE(value == 2);
    
    REQUIRE(queue.pop(value));
    REQUIRE(value == 3);

    REQUIRE(queue.pop(value));
    REQUIRE(value == 4);

    REQUIRE(queue.pop(value));
    REQUIRE(value == 5);

    REQUIRE_FALSE(queue.isFull());

    // _read and _write will now eventually wrap around.
    //        w        r
    // [6][7][_][_][_][6]
    REQUIRE(queue.push(6));
    REQUIRE(queue.push(7));

    REQUIRE_FALSE(queue.isFull());

    // Logical queue contents should now be:
    //
    // 6, 6, 7
    //
    REQUIRE(queue.pop(value));
    REQUIRE(value == 6);

    REQUIRE(queue.pop(value));
    REQUIRE(value == 6);

    REQUIRE(queue.pop(value));
    REQUIRE(value == 7);
    // _read and _write will now eventually wrap around.
    //       w/r        
    // [_][_][_][_][_][_]
    REQUIRE(queue.isEmpty());
    REQUIRE_FALSE(queue.isFull());
}


TEST_CASE("SpscQueue: full queue becomes writable after pop")
{
    constexpr size_t queueCapacity = 2;
    anasa::SpscQueue<int> queue(queueCapacity);

    // Usable capacity is only 2.
    REQUIRE(queue.push(10));
    REQUIRE(queue.push(20));

    REQUIRE_FALSE(queue.push(30));

    int value = 0;

    REQUIRE(queue.pop(value));
    REQUIRE(value == 10);

    REQUIRE(queue.push(30));

    REQUIRE(queue.pop(value));
    REQUIRE(value == 20);

    REQUIRE(queue.pop(value));
    REQUIRE(value == 30);

    REQUIRE(queue.isEmpty());
}


TEST_CASE("SpscQueue: repeated wrap-around preserves FIFO order")
{
    constexpr size_t queueCapacity = 3;
    anasa::SpscQueue<int> queue(queueCapacity);

    int expected = 0;

    for (int batch = 0; batch < 100; ++batch)
    {
        REQUIRE(queue.push(expected));
        REQUIRE(queue.push(expected + 1));
        REQUIRE(queue.push(expected + 2));

        int value = 0;

        REQUIRE(queue.pop(value));
        REQUIRE(value == expected);

        REQUIRE(queue.pop(value));
        REQUIRE(value == expected + 1);

        REQUIRE(queue.pop(value));
        REQUIRE(value == expected + 2);

        expected += 3;

        REQUIRE(queue.isEmpty());
    }
}

TEST_CASE("SpscQueue: reports full state")
{
    constexpr size_t queueCapacity = 3;

    anasa::SpscQueue<int> queue(queueCapacity);

    REQUIRE_FALSE(queue.isFull());

    REQUIRE(queue.push(1));
    REQUIRE_FALSE(queue.isFull());

    REQUIRE(queue.push(2));
    REQUIRE_FALSE(queue.isFull());

    REQUIRE(queue.push(3));
    REQUIRE(queue.isFull());

    int value = 0;

    REQUIRE(queue.pop(value));
    REQUIRE_FALSE(queue.isFull());
}

TEST_CASE("SpscQueue: producer and consumer can operate concurrently")
{
    constexpr size_t itemCount = 100000;
    constexpr size_t queueCapacity = 128;
    anasa::SpscQueue<int> queue(queueCapacity);

    std::vector<int> received;
    received.reserve(itemCount); // only capacity changes to itemCount and size is 0 now

    std::thread producer([&]()
    {
        for (int i = 0; i < itemCount; ++i)
        {
            // push() is non-blocking, so retry while the queue is full.
            while (!queue.push(i))
            {
                // give another runnable thread a chance to execute
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&]()
    {
        for (int i = 0; i < itemCount; ++i)
        {
            int value = 0;

            // pop() is non-blocking, so retry while the queue is empty.
            while (!queue.pop(value))
            {
                // give another runnable thread a chance to execute
                std::this_thread::yield();
            }

            received.push_back(value);
        }
    });

    producer.join();
    consumer.join();

    REQUIRE(received.size() == itemCount);

    for (int i = 0; i < itemCount; ++i)
        REQUIRE(received[i] == i);

    REQUIRE(queue.isEmpty());
}

#ifdef enable_benchmarks
TEST_CASE("SpscQueue throughput (benchmark)")
{
    // more ops / sec -> faster
    constexpr std::size_t queueCapacity = 4*8192;

    constexpr std::size_t operationPairs = 10000000;
    constexpr std::size_t count = 100;

    anasa::SpscQueue<int> queue{queueCapacity};

    int output = 0;

    // Warm-up
    for (std::size_t i = 0; i < 100000; ++i)
    {
        queue.push(42);
        queue.pop(output);
    }

    using Clock = std::chrono::steady_clock;

    double sumSeconds = 0.0;

    for (std::size_t c = 0; c < count; ++c)
    {
        const auto start = Clock::now();

        for (std::size_t i = 0; i < operationPairs; ++i)
        {
            queue.push(42);
            queue.pop(output);
        }

        const auto end = Clock::now();

        const double seconds = std::chrono::duration<double>(end - start).count();

        sumSeconds += seconds;
    }

    const double meanSecondsPerSample = sumSeconds / static_cast<double>(count);

    // Each iteration performs:
    // 1 push + 1 pop, therefore 2 queue operations.
    const double meanOperationsPerSecond = static_cast<double>(operationPairs * 2) / meanSecondsPerSample;

    std::cout<< "\nSpscQueue throughput\n" << "Mean: " << meanOperationsPerSecond / 1000000.0f << " M ops/sec\n";
}

#endif //enable_benchmarks