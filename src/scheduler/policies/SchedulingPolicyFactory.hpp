#ifndef SCHEDULING_POLICY_FACTORY_HPP
#define SCHEDULING_POLICY_FACTORY_HPP

#include <memory>

#include "scheduler/SchedulerTypes.hpp"
#include "scheduler/policies/ISchedulingPolicy.hpp"

namespace anasa
{
    std::shared_ptr<const ISchedulingPolicy> createSchedulingPolicy(SchedulingPolicyType policyType);

} // namespace anasa

#endif // SCHEDULING_POLICY_FACTORY_HPP