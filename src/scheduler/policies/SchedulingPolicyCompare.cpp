#include "scheduler/policies/SchedulingPolicyCompare.hpp"

#include <stdexcept>
#include <utility>

namespace anasa
{
    SchedulingPolicyCompare::SchedulingPolicyCompare(std::shared_ptr<const ISchedulingPolicy> schedulingPolicy)
        : _schedulingPolicy(std::move(schedulingPolicy))
    {
        if (_schedulingPolicy == nullptr)
            throw std::invalid_argument("schedulingPolicy must not be null");
    }

    bool SchedulingPolicyCompare::operator()(const PendingRenderTile& a, const PendingRenderTile& b) const
    {
        return _schedulingPolicy->hasLowerPrecedence(a, b);
    }

} // namespace anasa