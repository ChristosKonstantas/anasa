#ifndef AUDIO_QUEUE_HPP
#define AUDIO_QUEUE_HPP

#include "audio-pipeline/AudioConstants.hpp"
#include "audio-pipeline/AudioTypes.hpp"
#include "utils/queues/SpscQueue.hpp"

namespace anasa
{
    using ReadyAudioQueue = SpscQueue<AudioBlock, READY_AUDIO_QUEUE_SLOTS>;
} // namespace anasa

#endif // AUDIO_QUEUE_HPP