#ifndef AUDIO_BLOCK_PROCESSOR_HPP
#define AUDIO_BLOCK_PROCESSOR_HPP

#include "audio/IAudioBlockProcessor.hpp"
#include "audio/AudioTypes.hpp"
#include "playback/PlaybackState.hpp"
#include "utils/queues/SpscQueue.hpp"

namespace anasa
{
    class AudioBlockProcessor final : public IAudioBlockProcessor
    {
    public:
        // Both borrowed dependencies must outlive the processor.
        AudioBlockProcessor(int blockFrames, SharedState& sharedState, SpscQueue<AudioBlock>& readyAudioQueue);

        int blockFrames() const noexcept override;
        // Copy channel n to output n. Source and output channel counts must match.
        // Positive mismatched callback sizes produce silence without consuming or advancing.
        AudioProcessResult processBlock(AudioOutputBuffer output) noexcept override;

    private:
        int                     _blockFrames;
        AudioState              _audioState;
        SharedState&            _sharedState;
        SpscQueue<AudioBlock>&  _readyAudioQueue;
    };
}

#endif // AUDIO_BLOCK_PROCESSOR_HPP