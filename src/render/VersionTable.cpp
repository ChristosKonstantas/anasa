#include "render/VersionTable.hpp"
namespace anasa
{
    VersionTable::VersionTable(int count)
        : _count(count),
          _values(nullptr)
    {
        if(count <= 0)
            throw std::invalid_argument("Count must be positive");
       
        _values = std::make_unique<std::atomic<int>[]>(_count);

        for (int chunk = 0; chunk < _count; ++chunk)
            _values[chunk].store(1, std::memory_order_relaxed);
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