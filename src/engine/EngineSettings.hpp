#ifndef ENGINE_SETTINGS_HPP
#define ENGINE_SETTINGS_HPP

namespace anasa
{
    constexpr int SAMPLE_RATE = 48000;          // Sample frames per second.
    constexpr int AUDIO_BLOCK_FRAMES = 128;     // Sample frames per callback period. Each frame can contain N-channel data. For this project N = 1.
    constexpr int READY_QUEUE_SLOTS = 65;       // 64 usable audio blocks.
} // namespace anasa

#endif // ENGINE_SETTINGS_HPP