#include "scheduler/policies/PrioritySchedulingPolicy.hpp"

namespace anasa
{
    bool PrioritySchedulingPolicy::hasLowerPrecedence(const PendingRenderTile& a, const PendingRenderTile& b) const
    {
        // Order is:
        //      - Urgent before Visible
        //      - Visible before Background
        //      - Earlier urgent deadline first
        //      - Nearer chunk first
        //      - Older submission first
        if (a.priority != b.priority)
        {
            return static_cast<int>(a.priority) > static_cast<int>(b.priority);
        }

        if (a.priority == RenderPriority::Urgent && a.deadlineFrame != b.deadlineFrame)
            return a.deadlineFrame > b.deadlineFrame;

        if (a.distanceInFrames != b.distanceInFrames)
            return a.distanceInFrames > b.distanceInFrames;

        return a.sequence > b.sequence;
    }

    const char* PrioritySchedulingPolicy::name() const
    {
        return "Priority";
    }

} // namespace anasa