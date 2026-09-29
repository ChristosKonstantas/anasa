#ifndef BENCHMARK_HPP
#define BENCHMARK_HPP

#include <chrono>
#include <cstddef>
#include <stdexcept>

namespace anasa::benchmarks
{

struct BenchmarkResult
{
    double operationsPerSecond = 0.0;
    double nanosecondsPerOperation = 0.0;
    double checksum = 0.0;
};

class Benchmark
{
public:
    Benchmark(std::size_t operationsPerRound, std::size_t roundCount, std::size_t warmupOperations)
        : _operationsPerRound(operationsPerRound),
          _roundCount(roundCount),
          _warmupOperations(warmupOperations)
    {
        if (_operationsPerRound == 0)
            throw std::invalid_argument("operationsPerRound must be greater than zero");

        if (_roundCount == 0)
            throw std::invalid_argument("roundCount must be greater than zero");
    }

    template <typename Operation>
    BenchmarkResult run(Operation&& operation) const
    {
        using Clock = std::chrono::steady_clock;

        double checksum = 0.0f;

        for (std::size_t i = 0; i < _warmupOperations; ++i)
            checksum += operation(i);

        double totalSeconds = 0.0f;

        for (std::size_t round = 0; round < _roundCount; ++round)
        {
            const auto start = Clock::now();

            for (std::size_t i = 0; i < _operationsPerRound; ++i)
                checksum += operation(i);

            const auto end = Clock::now();
            totalSeconds += std::chrono::duration<double>(end - start).count();
        }

        const double meanSeconds = totalSeconds / static_cast<double>(_roundCount);
        const double operationsPerSecond = static_cast<double>(_operationsPerRound) / meanSeconds;
        const double nanosecondsPerOperation =  meanSeconds * 1000000000.0f / static_cast<double>(_operationsPerRound);

        return {operationsPerSecond, nanosecondsPerOperation, checksum};
    }

private:
    std::size_t _operationsPerRound;
    std::size_t _roundCount;
    std::size_t _warmupOperations;
};

} // namespace anasa::benchmarks

#endif // BENCHMARK_HPP