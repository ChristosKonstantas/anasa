#include "AudioBlockProcessor.hpp"
#include "playback/PlaybackTimeline.hpp"

#include <algorithm>
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
        if (output.frameCount <= 0)
            return {};

        if (output.frameCount == 0)
            return {};

        bool hasOutput = false;

        for (float* channel : output.channels)
        {
            if (channel == nullptr)
                continue;

            std::fill(channel, channel + output.frameCount, 0.0f);
            hasOutput = true;
        }

        if (!hasOutput || output.frameCount != _blockFrames)
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
            head->frameCount == _blockFrames &&
            head->samples.frameCount() >= _blockFrames &&
            static_cast<std::size_t>(head->samples.channelCount()) == output.channels.size();

        if (isExactBlock)
        {
            for (std::size_t channel = 0; channel < output.channels.size(); ++channel)
            {
                float* destination = output.channels[channel];

                if (destination == nullptr)
                    continue;

                const auto source = head->samples[static_cast<int>(channel)];

                for (int frame = 0; frame < _blockFrames; ++frame)
                    destination[frame] = source[frame];
            }

            // Copy before releasing the slot to the producer. pop() does not destroy it.
            _readyAudioQueue.pop();
        }

        _audioState.expectedBlockStartFrame += _blockFrames;
        _sharedState.nextUnconsumedFrame.store(_audioState.expectedBlockStartFrame, std::memory_order_release);

        return {true, !isExactBlock};
    }
}