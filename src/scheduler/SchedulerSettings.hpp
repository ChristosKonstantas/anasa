#ifndef SCHEDULER_SETTINGS_HPP
#define SCHEDULER_SETTINGS_HPP

#include "scheduler/SchedulerConstants.hpp"
#include "scheduler/SchedulerTypes.hpp"

namespace anasa
{
    struct SchedulerSettings
    {
        SchedulingPolicyType policyType = SchedulingPolicyType::Priority;

        int commandQueueSlots = DEFAULT_COMMAND_QUEUE_SLOTS;

        int prebufferBlocks = 16;
        int lowWaterBlocks = 16;
        int highWaterBlocks = 48;
        int urgentChunks = 8;

        int maxPendingTiles = 256;
        int urgentReservedTiles = 64;

        bool rebufferOnEdit = true;
    };

} // namespace anasa

#endif // SCHEDULER_SETTINGS_HPP