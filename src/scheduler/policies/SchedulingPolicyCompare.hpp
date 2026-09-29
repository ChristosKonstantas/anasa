#ifndef SCHEDULING_POLICY_COMPARE_HPP
#define SCHEDULING_POLICY_COMPARE_HPP

#include <memory>

#include "scheduler/policies/ISchedulingPolicy.hpp"

namespace anasa
{
    /*
    * Adapts ISchedulingPolicy to std::priority_queue's comparator contract.
    *
    * The shared_ptr keeps the selected policy alive when priority_queue copies its comparator internally.
    */
    class SchedulingPolicyCompare
    {
    public:
        explicit SchedulingPolicyCompare(std::shared_ptr<const ISchedulingPolicy> schedulingPolicy);

        bool operator()(const PendingRenderTile& a, const PendingRenderTile& b) const;

    private:
        std::shared_ptr<const ISchedulingPolicy> _schedulingPolicy;
    };

} // namespace anasa

#endif // SCHEDULING_POLICY_COMPARE_HPP