#ifndef ENGINE_SETTINGS_HPP
#define ENGINE_SETTINGS_HPP

#include "audio/AudioSettings.hpp"
#include "render/RenderSettings.hpp"
#include "execution/ExecutorSettings.hpp"
#include "scheduler/SchedulerSettings.hpp"

namespace anasa
{
    struct EngineSettings
    {
        AudioSettings audio;
        RenderSettings render;
        ExecutorSettings executor;
        SchedulerSettings scheduler;
        int timelineFrames = 0;
        int timelineInSeconds = 12;
        int captureFrames = 0; // Zero disables simulated callback recording.
    };
    
} // namespace anasa

#endif // ENGINE_SETTINGS_HPP