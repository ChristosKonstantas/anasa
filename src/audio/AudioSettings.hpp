#ifndef AUDIO_SETTINGS_HPP
#define AUDIO_SETTINGS_HPP

namespace anasa
{
    struct AudioSettings
    {
        int sampleRate       = 48000; // Sample frames per second.
        int audioBlockFrames = 128; // Internal mono block size and the simulator callback size for now.
        int channelCount     = 1; // Output channels: rendered source content is currently mono.
    };
    
} // namespace anasa

#endif // AUDIO_SETTINGS_HPP