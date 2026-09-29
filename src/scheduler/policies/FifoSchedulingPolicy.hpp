#ifndef FIFO_SCHEDULING_POLICY_HPP
#define FIFO_SCHEDULING_POLICY_HPP

#include "scheduler/policies/ISchedulingPolicy.hpp"

namespace anasa
{
    class FifoSchedulingPolicy final : public ISchedulingPolicy
    {
    public:
        bool hasLowerPrecedence(const PendingRenderTile& a, const PendingRenderTile& b) const override;

        const char* name() const override;
    };

} // namespace anasa

#endif // FIFO_SCHEDULING_POLICY_HPP