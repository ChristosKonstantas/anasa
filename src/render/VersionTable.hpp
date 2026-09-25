#ifndef VERSION_TABLE_HPP
#define VERSION_TABLE_HPP

#include <memory>
#include <atomic>
#include <stdexcept>
#include <cassert>

#include "render/IChunkVersionReader.hpp"

namespace anasa
{
    // Version table is needed because:
    // The scheduler may request a render job, but the user can edit the timeline before that job finishes.
    class VersionTable final : public IChunkVersionReader
    {
    public:
        explicit VersionTable(int count);

        int get(int chunk) const override;

        int bump(int chunk);

        int count() const override;

    private:
        int _count;
        std::unique_ptr<std::atomic<int>[]> _values; // dynamically allocated array containing one atomic version number per timeline chunk
    };

} // namespace anasa

#endif //VERSION_TABLE_HPP