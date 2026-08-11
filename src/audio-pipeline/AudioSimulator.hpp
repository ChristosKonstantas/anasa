#ifndef AUDIO_SIMULATOR_HPP
#define AUDIO_SIMULATOR_HPP

#include <thread>

#include "engine/EngineTypes.hpp"
#include "engine/EngineSettings.hpp"
#include "utils/queues/SpscQueue.hpp"

namespace anasa
{
    using ReadyAudioQueue = SpscQueue<AudioBlock, READY_QUEUE_SLOTS>;
    
    class AudioSimulator
    {
    public:
        AudioSimulator(SharedState& sharedState, SpscQueue<AudioBlock, READY_QUEUE_SLOTS>& readyAudioQueue);
        ~AudioSimulator();

        void                            start();
        void                            stop();
                      
        long long                       getCallbacksCount() const;
        long long                       getUnderrunsCount() const;
        long long                       getCallbackMaxInUs() const;
        double                          getChecksum() const;
                      
    private:                      
        void                            periodicAudioDeviceClock();
        void                            audioCallback();

        std::thread                     _audioThread;
        std::atomic<bool>               _stopRequested;
        bool                            _started;
        long long                       _callbacks;
        long long                       _underruns;
        long long                       _callbackMaxInUs;
        double                          _checksum;
        AudioState                      _audioState;
        SharedState&                    _sharedState;
        ReadyAudioQueue&                _readyAudioQueue;
    };
} // namespace anasa

#endif // AUDIO_SIMULATOR_HPP