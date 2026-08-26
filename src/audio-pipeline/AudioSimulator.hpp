#ifndef AUDIO_SIMULATOR_HPP
#define AUDIO_SIMULATOR_HPP

#include <atomic>
#include <thread>

#include "utils/queues/SpscQueue.hpp"
#include "audio-pipeline/AudioSettings.hpp"
#include "audio-pipeline/AudioTypes.hpp"
#include "playback/PlaybackState.hpp"
#include "playback/PlaybackTimeline.hpp"

namespace anasa
{
    class AudioSimulator
    {
    public:
        AudioSimulator(AudioSettings settings, SharedState& sharedState, SpscQueue<AudioBlock>& readyAudioQueue);
        ~AudioSimulator();

        void                     start();
        void                     stop();
        long long                getCallbacksCount() const;
        long long                getUnderrunsCount() const;
        long long                getCallbackMaxInUs() const;
        double                   getChecksum() const;
        
    private:               
        void                     periodicAudioDeviceClock();
        void                     audioCallback();
        
        AudioSettings            _settings;
        std::thread              _audioThread;
        std::atomic<bool>        _stopRequested;
        bool                     _started;
        long long                _callbacks;
        long long                _underruns;
        long long                _callbackMaxInUs;
        double                   _checksum;
        AudioState               _audioState;
        SharedState&             _sharedState;
        SpscQueue<AudioBlock>&   _readyAudioQueue;
    };
} // namespace anasa

#endif // AUDIO_SIMULATOR_HPP