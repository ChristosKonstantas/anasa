anasa_add_module(
    scheduler
    SOURCES
        Scheduler.hpp
        Scheduler.cpp
        SchedulerConstants.hpp
        SchedulerSettings.hpp
        SchedulerTypes.hpp
)

target_link_libraries(scheduler PUBLIC audio-pipeline executor playback queues renderer)