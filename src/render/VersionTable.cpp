#include "render/VersionTable.hpp"
namespace anasa
{
    VersionTable::VersionTable(int count)
        : _count(count),
          _values(std::make_unique<std::atomic<int>[]>(count))
    {
        assert(count > 0);
        for (int i = 0; i < _count; ++i)
            _values[i].store(1);
    }

    int VersionTable::get(int chunk) const
    {
        assert(chunk >= 0);
        assert(chunk < _count);
        return _values[chunk].load(std::memory_order_acquire);
    }

    int VersionTable::bump(int chunk)
    {
        assert(chunk >= 0);
        assert(chunk < _count);
        return _values[chunk].fetch_add(1, std::memory_order_acq_rel) + 1;
    }

    int VersionTable::count() const
    {
        return _count;
    }

} //namespace anasa