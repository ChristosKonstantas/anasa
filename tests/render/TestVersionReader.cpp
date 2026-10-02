#include "TestVersionReader.hpp"

namespace anasa
{
    TestVersionReader::TestVersionReader(int currentReads) 
        : _currentReads(currentReads) 
    {}
    
    int TestVersionReader::get(int chunk) const
    {
        if (chunk != 0)
            throw std::out_of_range("invalid test chunk");

        return _reads.fetch_add(1, std::memory_order_relaxed) < _currentReads ? 7 : 8;
    }
    
    int TestVersionReader::count() const
    { 
        return 1;
    }

} // namespace anasa