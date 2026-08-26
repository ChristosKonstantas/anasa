#ifndef SCHEDULER_TYPES_HPP
#define SCHEDULER_TYPES_HPP

namespace anasa
{
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

} // namespace anasa

#endif // SCHEDULER_TYPES_HPP