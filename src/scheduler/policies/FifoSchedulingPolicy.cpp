#include "scheduler/policies/FifoSchedulingPolicy.hpp"

namespace anasa
{
    bool FifoSchedulingPolicy::hasLowerPrecedence(const PendingRenderTile& a, const PendingRenderTile& b) const
    {
        return a.sequence > b.sequence;
    }

    const char* FifoSchedulingPolicy::name() const
    {
        return "FIFO";
    }

} // namespace anasa