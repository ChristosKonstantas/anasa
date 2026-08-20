#ifndef RENDER_SETTINGS_HPP
#define RENDER_SETTINGS_HPP

namespace anasa
{

struct RenderSettings
{
    // Synthetic serialized CPU work used to create measurable
    // scheduling contention. This is not production DSP or inference.
    int workIterations = 300;
};

} // namespace anasa

#endif // RENDER_SETTINGS_HPP