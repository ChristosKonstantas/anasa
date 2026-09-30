#ifndef AUDIO_SETTINGS_HPP
#define AUDIO_SETTINGS_HPP

namespace anasa
{
    struct AudioSettings
    {
        int sampleRate       = 48000; // Sample frames per second.
        int audioBlockFrames = 128;   // Frames per internal block and simulator callback.
        int channelCount     = 2;     // Channels throughout rendering, caching, publication and output.
    };
    
} // namespace anasa

#endif // AUDIO_SETTINGS_HPP