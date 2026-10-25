#ifndef TIMELINE_AUDIO_PUBLISHER_HPP
#define TIMELINE_AUDIO_PUBLISHER_HPP


#include "audio/AudioTypes.hpp"
#include "scheduler/IRenderCacheReader.hpp"
#include "utils/queues/SpscQueue.hpp"

namespace anasa
{
    class TimelineAudioPublisher
    {
    public:
        TimelineAudioPublisher(int audioBlockFrames, int channelCount, int totalFrames, SpscQueue<AudioBlock>& readyAudioQueue);

        void reset(int targetFrame) noexcept;

        void publish(int nextUnconsumedFrame, int generation, const IRenderCacheReader& cache);

        int  readyLeadBlocks(int nextUnconsumedFrame) const noexcept;

        bool entireRemainderPublished() const noexcept;

    private:
        const int              _audioBlockFrames;
        const int              _channelCount;
        const int              _totalFrames;
        SpscQueue<AudioBlock>& _readyAudioQueue;
        int                    _nextFrameToPublish = 0;
    };

} // namespace anasa

#endif // TIMELINE_AUDIO_PUBLISHER_HPP