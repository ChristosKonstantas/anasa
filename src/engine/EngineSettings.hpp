#ifndef ENGINE_SETTINGS_HPP
#define ENGINE_SETTINGS_HPP

#include "audio-pipeline/AudioSettings.hpp"
#include "render/RenderSettings.hpp"

namespace anasa
{
    struct EngineSettings
    {
        AudioSettings audio;
        RenderSettings render;
        int timelineInSeconds = 12;
    };
    
} // namespace anasa

#endif // ENGINE_SETTINGS_HPP