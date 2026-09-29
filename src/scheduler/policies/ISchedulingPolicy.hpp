#ifndef I_SCHEDULING_POLICY_HPP
#define I_SCHEDULING_POLICY_HPP

#include "scheduler/SchedulerTypes.hpp"

namespace anasa
{
    /*
     * Strategy interface controlling pending render-tile ordering.
     *
     * Implementations must define a strict weak ordering and must not depend
     * on mutable state that changes while tiles are inside the priority queue.
     */
    class ISchedulingPolicy
    {
    public:
        virtual ~ISchedulingPolicy() = default;

        /* Returns true when a has lower queue precedence than b, meaning b should be dispatched before a */
        virtual bool hasLowerPrecedence(const PendingRenderTile& a, const PendingRenderTile& b) const = 0;

        virtual const char* name() const = 0;
    };

} // namespace anasa

#endif // I_SCHEDULING_POLICY_HPP