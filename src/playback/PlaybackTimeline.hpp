#ifndef PLAYBACK_TIMELINE_HPP
#define PLAYBACK_TIMELINE_HPP

#include <algorithm>
#include <cassert>

namespace anasa
{
    inline int clampTimelineFrame(int frame, int totalFrames)
    {
        assert(totalFrames > 0);

        return std::max(0, std::min(frame, totalFrames - 1));
    }

    inline int clampTimelineBoundary(int frame, int totalFrames)
    {
        assert(totalFrames > 0);

        return std::max(0, std::min(frame, totalFrames));
    }

    inline int alignFrameToAudioBlock(int frame, int audioBlockFrames)
    {
        assert(frame >= 0);
        assert(audioBlockFrames > 0);

        return frame - frame % audioBlockFrames;
    }

} // namespace anasa

#endif // PLAYBACK_TIMELINE_HPP