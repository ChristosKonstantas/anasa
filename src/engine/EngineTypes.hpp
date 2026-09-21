#ifndef ENGINE_TYPES_HPP
#define ENGINE_TYPES_HPP

namespace anasa
{
    struct PlaybackSnapshot
    {
        bool playing;
        int generation;
        int cursorGeneration;
        int nextUnconsumedFrame;
    };

    struct EngineMetrics
    {
        long long callbacks;
        long long underruns;
        long long maximumCallbackMicroseconds;
    };

}// namespace anasa

#endif // ENGINE_TYPES_HPP