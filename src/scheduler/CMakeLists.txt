anasa_add_module(
    scheduler
    SOURCES
        TimelineAudioPublisher.hpp
        TimelineAudioPublisher.cpp
        Scheduler.hpp
        Scheduler.cpp
        SchedulerConstants.hpp
        SchedulerSettings.hpp
        SchedulerTypes.hpp
        policies/ISchedulingPolicy.hpp
        policies/FifoSchedulingPolicy.hpp
        policies/FifoSchedulingPolicy.cpp
        policies/PrioritySchedulingPolicy.hpp
        policies/PrioritySchedulingPolicy.cpp
        policies/SchedulingPolicyCompare.hpp
        policies/SchedulingPolicyCompare.cpp
        policies/SchedulingPolicyFactory.hpp
        policies/SchedulingPolicyFactory.cpp
)

target_link_libraries(scheduler PUBLIC audio-contracts executor-contracts playback queues renderer)