#ifndef SCHEDULER_TYPES_HPP
#define SCHEDULER_TYPES_HPP

#include <memory>

namespace anasa
{
    struct RenderJob; // forward declaration

    /* Selects the scheduling Strategy that Scheduler will construct */
    enum class SchedulingPolicyType
    {
        Fifo,
        Priority
    };

    /* Commands sent from the Engine/control thread to the Scheduler thread */
    enum class CommandType
    {
        Play,
        Pause,
        Seek,
        SetViewport,
        Edit,
        Stop
    };

    struct Command
    {
        CommandType type = CommandType::Pause;

        /*
         * Seek:
         *   firstFrame is the requested playback position.
         *
         * SetViewport:
         *   [firstFrame, lastFrame) is the visible timeline region.
         *
         * Edit:
         *   [firstFrame, lastFrame] is the edited timeline region.
         *
         * Play, Pause and Stop:
         *   both frame fields are ignored.
         */
        int firstFrame = 0;
        int lastFrame = 0;
    };

    enum class RenderPriority
    {
        Urgent = 0,
        Visible = 1,
        Background = 2
    };

    struct RenderClassification // scheduling metadata
    {
        RenderPriority priority = RenderPriority::Background;

        // Timeline frame at which urgent content is first required.
        // -1 means that the job has no playback deadline.
        int deadlineFrame = -1;

        // Distance between the chunk start and the current playhead.
        int distanceInFrames = 0;
    };

    /* One independently schedulable tile belonging to a RenderJob. The Scheduler owns these objects until they are submitted to Executor.*/
    struct PendingRenderTile
    {
        std::shared_ptr<RenderJob> job;
        int                        tileIndex = 0;
        RenderPriority             priority = RenderPriority::Background;
        int                        deadlineFrame = -1;
        int                        distanceInFrames = 0;
        long long                  sequence = 0;
    };

} // namespace anasa

#endif // SCHEDULER_TYPES_HPP