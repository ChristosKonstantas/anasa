#include "scheduler/TimelineAudioPublisher.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>

#include "audio/AudioConstants.hpp"
#include "render/RenderFrameUtils.hpp"

namespace anasa
{
    TimelineAudioPublisher::TimelineAudioPublisher(int audioBlockFrames, int channelCount, int totalFrames, SpscQueue<AudioBlock>& readyAudioQueue)
        : _audioBlockFrames(audioBlockFrames),
          _channelCount(channelCount),
          _totalFrames(totalFrames),
          _readyAudioQueue(readyAudioQueue)
    {
        if (_channelCount <= 0)
            throw std::invalid_argument("channelCount must be greater than zero");

        if (_audioBlockFrames <= 0)
            throw std::invalid_argument("audioBlockFrames must be greater than zero");

        if (_audioBlockFrames > MAX_AUDIO_BLOCK_FRAMES)
            throw std::invalid_argument("audioBlockFrames exceeds MAX_AUDIO_BLOCK_FRAMES");

        if (CHUNK_FRAMES % _audioBlockFrames != 0)
            throw std::invalid_argument("audioBlockFrames must divide CHUNK_FRAMES");

        if (_totalFrames <= 0)
            throw std::invalid_argument("totalFrames must be greater than zero");

        if (_totalFrames % _audioBlockFrames != 0)
            throw std::invalid_argument("totalFrames must be divisible by audioBlockFrames - current engine does not support a partial final audio block");
    }

    void TimelineAudioPublisher::reset(int targetFrame) noexcept
    {
        assert(targetFrame >= 0 && targetFrame <= _totalFrames);
        assert(targetFrame % _audioBlockFrames == 0);
        _nextFrameToPublish = targetFrame;
    }

    void TimelineAudioPublisher::publish(int nextUnconsumedFrame, int generation, std::span<const CacheEntry> cache, const IChunkVersionReader& versions)
    {
        assert(nextUnconsumedFrame >= 0 && nextUnconsumedFrame <= _totalFrames);
        assert(nextUnconsumedFrame % _audioBlockFrames == 0);
        assert(cache.size() == static_cast<std::size_t>(1 + (_totalFrames - 1) / CHUNK_FRAMES));
        assert(cache.size() == static_cast<std::size_t>(versions.count()));

        // Audio already passed by the consumer must not be published late.
        _nextFrameToPublish = std::max(_nextFrameToPublish, nextUnconsumedFrame);

        while (_nextFrameToPublish <= _totalFrames - _audioBlockFrames)
        {
            const int chunk = frameToChunk(_nextFrameToPublish);
            const CacheEntry& cacheEntry = cache[chunk];

            // Never skip missing or outdated content to publish a later chunk.
            if (cacheEntry.version != versions.get(chunk))
                break;

            const int blockFirstFrame = _nextFrameToPublish;
            const int chunkOffset = blockFirstFrame - firstFrameOfChunk(chunk);

            assert(cacheEntry.samples.channelCount() == _channelCount);
            assert(chunkOffset + _audioBlockFrames <= cacheEntry.samples.frameCount());

            const bool pushed = _readyAudioQueue.pushWith([this, &cacheEntry, generation, blockFirstFrame, chunkOffset](AudioBlock& block)
            {
                assert(block.samples.channelCount() == _channelCount);
                assert(block.samples.frameCount() >= _audioBlockFrames);

                block.generation = generation;
                block.firstFrame = blockFirstFrame;
                block.frameCount = _audioBlockFrames;

                for (int channel = 0; channel < _channelCount; ++channel)
                {
                    const std::span<const float> source = cacheEntry.samples[channel];
                    const std::span<float> destination = block.samples[channel];

                    for (int frame = 0; frame < _audioBlockFrames; ++frame)
                        destination[frame] = source[chunkOffset + frame];
                }
            });

            if (!pushed)
                break;

            _nextFrameToPublish += _audioBlockFrames;
        }
    }

    int TimelineAudioPublisher::readyLeadBlocks(int nextUnconsumedFrame) const noexcept
    {
        assert(nextUnconsumedFrame >= 0 && nextUnconsumedFrame <= _totalFrames);
        return std::max(0, _nextFrameToPublish - nextUnconsumedFrame) / _audioBlockFrames;
    }

    bool TimelineAudioPublisher::entireRemainderPublished() const noexcept
    {
        return _nextFrameToPublish >= _totalFrames;
    }
} // namespace anasa