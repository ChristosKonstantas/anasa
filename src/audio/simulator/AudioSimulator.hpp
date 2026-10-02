#ifndef AUDIO_SIMULATOR_HPP
#define AUDIO_SIMULATOR_HPP

#include <vector>
#include <atomic>
#include <thread>

#include "audio/AudioConstants.hpp"
#include "audio/AudioSettings.hpp"
#include "audio/IAudioBlockProcessor.hpp"
#include "audio/AudioBuffer.hpp"

namespace anasa
{
    class AudioSimulator
    {
    public:
        // Borrowed dependencies must outlive the simulator and its worker thread.
        AudioSimulator(AudioSettings settings, const std::atomic<bool>& engineStop, IAudioBlockProcessor& processor);
        ~AudioSimulator();

        AudioSimulator(const AudioSimulator&) = delete;
        AudioSimulator& operator=(const AudioSimulator&) = delete;
        AudioSimulator(AudioSimulator&&) = delete;
        AudioSimulator& operator=(AudioSimulator&&) = delete;

        // Called serially by the owner, outside the audio thread.
        void                     start();
        void                     stop();

        // Read metrics only while stopped (they accumulate across restarts).
        long long                getCallbacksCount() const;
        long long                getUnderrunsCount() const;
        long long                getCallbackMaxInUs() const;
        double                   getChecksum() const;

    private:
        void                     periodicAudioDeviceClock();
        void                     audioCallback();

        AudioSettings                             _settings;
        const std::atomic<bool>&                  _engineStop;
        IAudioBlockProcessor&                     _processor;
        AudioBuffer                               _output;
        std::vector<float*>                       _outputChannels;
        std::thread                               _audioThread;
        std::atomic<bool>                         _stopRequested{false};
        bool                                      _started = false;
        long long                                 _callbacks = 0;
        long long                                 _underruns = 0;
        long long                                 _callbackMaxInUs = 0;
        double                                    _checksum = 0.0;
    };
}

#endif // AUDIO_SIMULATOR_HPP