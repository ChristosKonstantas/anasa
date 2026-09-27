#include "AudioBlockProcessor.hpp"
#include "playback/PlaybackTimeline.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace anasa
{
    static_assert(std::atomic<bool>::is_always_lock_free);
    static_assert(std::atomic<int>::is_always_lock_free);

    AudioBlockProcessor::AudioBlockProcessor(int blockFrames, SharedState& sharedState, SpscQueue<AudioBlock>& readyAudioQueue)
        : _blockFrames(blockFrames),
          _sharedState(sharedState),
          _readyAudioQueue(readyAudioQueue)
    {
        if (_blockFrames <= 0 || _blockFrames > MAX_AUDIO_BLOCK_FRAMES)
            throw std::invalid_argument("Invalid internal audio block size");
    }

    int AudioBlockProcessor::blockFrames() const noexcept
    {
        return _blockFrames;
    }

    AudioProcessResult AudioBlockProcessor::processBlock(AudioOutputBuffer output) noexcept    
    {
        if (output.frameCount == 0)
            return {};

        assert(output.frameCount == _blockFrames);

        bool hasOutput = false;

        for (float* channel : output.channels)
        {
            if (channel == nullptr)
                continue;

            std::fill(channel, channel + output.frameCount, 0.0f);
            hasOutput = true;
        }

        if (!hasOutput)
            return {};

        if (_sharedState.stop.load(std::memory_order_acquire))
            return {};

        const int globalGeneration = _sharedState.generation.load(std::memory_order_acquire);

        if (_audioState.generation != globalGeneration)
        {
            _audioState.generation = globalGeneration;
            _audioState.expectedBlockStartFrame = alignFrameToAudioBlock(_sharedState.targetFrame.load(std::memory_order_relaxed), _blockFrames);

            _sharedState.nextUnconsumedFrame.store(_audioState.expectedBlockStartFrame, std::memory_order_relaxed);
            _sharedState.audioCursorGeneration.store(globalGeneration, std::memory_order_release);
        }

        const AudioBlock* head = nullptr;

        // Bound stale-block cleanup even if the producer keeps publishing.
        for (std::size_t i = 0; i < _readyAudioQueue.capacity(); ++i)
        {
            head = _readyAudioQueue.front();

            if (head == nullptr)
                break;

            const bool outdatedGeneration = head->generation < globalGeneration;
            const bool oldFrame = head->generation == globalGeneration && head->firstFrame < _audioState.expectedBlockStartFrame;

            if (!outdatedGeneration && !oldFrame)
                break;

            _readyAudioQueue.pop();
            head = nullptr;
        }

        if (!_sharedState.playing.load(std::memory_order_acquire))
            return {};

        const bool isExactBlock =
            head != nullptr &&
            head->generation == globalGeneration &&
            head->firstFrame == _audioState.expectedBlockStartFrame &&
            head->frameCount == _blockFrames;

        if (isExactBlock)
        {
            for (float* channel : output.channels)
            {
                if (channel == nullptr)
                    continue;

                for (int frame = 0; frame < _blockFrames; ++frame)
                    channel[frame] = head->samples[frame];
            }

            // Copy before releasing the slot to the producer. pop() does not destroy it.
            _readyAudioQueue.pop();
        }

        _audioState.expectedBlockStartFrame += _blockFrames;
        _sharedState.nextUnconsumedFrame.store(_audioState.expectedBlockStartFrame, std::memory_order_release);

        return {true, !isExactBlock};
    }
}