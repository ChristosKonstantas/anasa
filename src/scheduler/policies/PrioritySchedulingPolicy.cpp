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
        if (a.classification.priority != b.classification.priority)
        {
            return static_cast<int>(a.classification.priority) > static_cast<int>(b.classification.priority);
        }

        if (a.classification.priority == RenderPriority::Urgent && a.classification.deadlineFrame != b.classification.deadlineFrame)
            return a.classification.deadlineFrame > b.classification.deadlineFrame;

        if (a.classification.distanceInFrames != b.classification.distanceInFrames)
            return a.classification.distanceInFrames > b.classification.distanceInFrames;

        return a.sequence > b.sequence;
    }

    const char* PrioritySchedulingPolicy::name() const
    {
        return "Priority";
    }

} // namespace anasa