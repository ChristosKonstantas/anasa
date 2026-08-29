#include "scheduler/policies/SchedulingPolicyFactory.hpp"

#include <stdexcept>

#include "scheduler/policies/FifoSchedulingPolicy.hpp"
#include "scheduler/policies/PrioritySchedulingPolicy.hpp"

namespace anasa
{
    std::shared_ptr<const ISchedulingPolicy> createSchedulingPolicy(SchedulingPolicyType policyType)
    {
        if (policyType == SchedulingPolicyType::Fifo)
            return std::make_shared<FifoSchedulingPolicy>();

        if (policyType == SchedulingPolicyType::Priority)
            return std::make_shared<PrioritySchedulingPolicy>();

        throw std::invalid_argument("unsupported scheduling policy");
    }

} // namespace anasa