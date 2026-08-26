#ifndef RENDER_SETTINGS_HPP
#define RENDER_SETTINGS_HPP

namespace anasa
{
    struct RenderSettings
    {
        /*
         * Number of neighboring frames affected by an edit. 
         * This models the contextual dependency of a DSP algorithm or neural model around the directly edited range.
        */
        int contextFrames = 128;

        /* Synthetic serialized CPU work used to create measurable scheduling contention */
        int workIterations = 300;
    };

} // namespace anasa

#endif // RENDER_SETTINGS_HPP