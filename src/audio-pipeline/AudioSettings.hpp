#ifndef AUDIO_SETTINGS_HPP
#define AUDIO_SETTINGS_HPP

namespace anasa
{
    struct AudioSettings
    {
        int sampleRate       = 48000; // Sample frames per second.
        int audioBlockFrames = 128;   // Sample frames per callback period. Each frame can contain N-channel data. For this project N = 1.
    };
    
} // namespace anasa

#endif // AUDIO_SETTINGS_HPP