#ifndef BENCHMARK_HPP
#define BENCHMARK_HPP

#include <chrono>
#include <cstddef>

namespace anasa::benchmarks
{

struct BenchmarkResult
{
    double transfersPerSecond = 0.0;
    double nanosecondsPerTransfer = 0.0;
    double checksum = 0.0;
};

class Benchmark
{
public:
    Benchmark(std::size_t transfersPerRound, std::size_t roundCount, std::size_t warmupTransfers)
        : _transfersPerRound(transfersPerRound),
          _roundCount(roundCount),
          _warmupTransfers(warmupTransfers)
    {
    }

    template <typename Transfer>
    BenchmarkResult run(Transfer&& transfer) const
    {
        using Clock = std::chrono::steady_clock;

        double checksum = 0.0f;

        for (std::size_t i = 0; i < _warmupTransfers; ++i)
            checksum += transfer(i);

        double totalSeconds = 0.0f;

        for (std::size_t round = 0; round < _roundCount; ++round)
        {
            const auto start = Clock::now();

            for (std::size_t i = 0; i < _transfersPerRound; ++i)
                checksum += transfer(i);

            const auto end = Clock::now();
            totalSeconds += std::chrono::duration<double>(end - start).count();
        }

        const double meanSeconds = totalSeconds / static_cast<double>(_roundCount);
        const double transfersPerSecond = static_cast<double>(_transfersPerRound) / meanSeconds;
        const double nanosecondsPerTransfer =  meanSeconds * 1000000000.0f / static_cast<double>(_transfersPerRound);

        return {transfersPerSecond, nanosecondsPerTransfer, checksum};
    }

private:
    std::size_t _transfersPerRound;
    std::size_t _roundCount;
    std::size_t _warmupTransfers;
};

} // namespace anasa::benchmarks

#endif // BENCHMARK_HPP