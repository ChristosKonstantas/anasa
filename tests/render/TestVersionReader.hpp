#ifndef TEST_VERSION_READER_HPP
#define TEST_VERSION_READER_HPP

#include <atomic>
#include <stdexcept>

#include "render/IChunkVersionReader.hpp"

namespace anasa
{
    // Deterministically models an edit after a configured number of reads.
    // Atomic bookkeeping keeps this reader safe for concurrent callers too.
    class TestVersionReader final : public IChunkVersionReader
    {
    public:
        explicit TestVersionReader(int currentReads);

        int      get(int chunk) const override;

        int      count() const override;

    private:
        int                      _currentReads;
        mutable std::atomic<int> _reads{0};
    };
} // namespace anasa

#endif // TEST_VERSION_READER_HPP