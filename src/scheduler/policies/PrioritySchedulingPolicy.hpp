#ifndef PRIORITY_SCHEDULING_POLICY_HPP
#define PRIORITY_SCHEDULING_POLICY_HPP

#include "scheduler/policies/ISchedulingPolicy.hpp"

namespace anasa
{
    class PrioritySchedulingPolicy final : public ISchedulingPolicy
    {
    public:
        bool hasLowerPrecedence(const PendingRenderTile& a, const PendingRenderTile& b) const override;
        
        const char* name() const override;
    };

} // namespace anasa

#endif // PRIORITY_SCHEDULING_POLICY_HPP