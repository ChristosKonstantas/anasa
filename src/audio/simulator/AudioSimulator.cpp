#include "AudioSimulator.hpp"
#include "audio/AudioConstants.hpp"

#include <stdexcept>
#include <chrono>
#include <thread>

namespace anasa 
{
    using Clock = std::chrono::steady_clock;
    AudioSimulator::AudioSimulator(AudioSettings settings, const std::atomic<bool>& engineStop, IAudioBlockProcessor& processor)
        : _settings(settings),
          _engineStop(engineStop),
          _processor(processor)
    {
        if (_settings.sampleRate <= 0)
            throw std::invalid_argument("sampleRate must be greater than zero");

        if (_settings.audioBlockFrames <= 0 || _settings.audioBlockFrames > MAX_AUDIO_BLOCK_FRAMES)
            throw std::invalid_argument("Invalid simulated callback size");

        if (_settings.channelCount <= 0)
            throw std::invalid_argument("channelCount must be greater than zero");

        if (_settings.audioBlockFrames != _processor.blockFrames())
            throw std::invalid_argument("Simulator and processor block sizes must match");

        // Allocate output storage before the audio thread starts.
        _output = AudioBuffer(_settings.channelCount, _settings.audioBlockFrames);
        _outputChannels.resize(static_cast<std::size_t>(_settings.channelCount));

        for (int channel = 0; channel < _settings.channelCount; ++channel)
        {
            _outputChannels[channel] = _output.channel(channel).data();
        }
    }

    AudioSimulator::~AudioSimulator()
    {
        stop();
    }

    void AudioSimulator::start()
    {
        if (_started)
            return;

        _stopRequested.store(false, std::memory_order_release);

        _audioThread = std::thread(&AudioSimulator::periodicAudioDeviceClock, this);

        _started = true;
    }

    void AudioSimulator::stop()
    {
        if (!_started)
            return;

        _stopRequested.store(true, std::memory_order_release);

        if (_audioThread.joinable())
            _audioThread.join();

        _started = false;
    }

    long long AudioSimulator::getCallbacksCount() const
    {
        return _callbacks;
    }

    long long AudioSimulator::getUnderrunsCount() const
    {
        return _underruns;
    }

    long long AudioSimulator::getCallbackMaxInUs() const
    {
        return _callbackMaxInUs;
    }

    double AudioSimulator::getChecksum() const
    {
        return _checksum;
    }

    void AudioSimulator::periodicAudioDeviceClock() 
    {
        /* The function implements a periodic virtual audio-device clock and decides when callbacks should occur */

        // calculate the duration of one audio block 
        Clock::duration period = std::chrono::duration_cast<Clock::duration>
        (std::chrono::duration<double>(static_cast<double>(_settings.audioBlockFrames) / static_cast<double>(_settings.sampleRate)));

        // If the engine starts now, the first callback is scheduled for now + period
        Clock::time_point nextCallbackTime = Clock::now() + period;
        
        // Audio thread continues until another thread requests shutdown
        while (!_stopRequested.load(std::memory_order_acquire) && !_engineStop.load(std::memory_order_acquire)) 
        {
            // The simulated audio thread has no block to consume yet, so it sleeps.
            // At nextCallbackTime, another audio block becomes due for consumption.
            std::this_thread::sleep_until(nextCallbackTime);

            // Shutdown may be requested while the audio thread is sleeping. 
            // Without this check, the thread would wake and execute one additional callback after stop() had been requested.
            if (_stopRequested.load(std::memory_order_acquire) || _engineStop.load(std::memory_order_acquire))
                break;

            // at this point the the operating system actually woke the thread
            // !! -> OS may wake the thread later than requested
            // depending on how many msecs is the block duration several callback periods may already be due
            Clock::time_point wakeTime = Clock::now();

            // Process every audio block that became due while the thread slept.
            do // this loop executes at least one callback because the thread was sleeping until a callback deadline
            {
                Clock::time_point callbackStart = Clock::now();

                audioCallback();    

                long long callbackDurationInUsecs = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - callbackStart).count();

                _callbackMaxInUs = _callbackMaxInUs > callbackDurationInUsecs ? _callbackMaxInUs : callbackDurationInUsecs;

                // Advance virtual device clock from the previous scheduled deadline
                // NOTE: Clock::now() + period would accumulate timing drift whenever a wake-up was late ->
                // -> late wake-up should not permanently move all future deadlines
                nextCallbackTime += period;

            // catch-up missed callback periods
            } while(nextCallbackTime <= wakeTime && 
                    !_stopRequested.load(std::memory_order_acquire) &&
                    !_engineStop.load(std::memory_order_acquire));
        }
    }

    void AudioSimulator::audioCallback()
    {
        AudioOutputBuffer output{_outputChannels, _settings.audioBlockFrames};
        const AudioProcessResult result = _processor.processBlock(output);

        if (!result.playing)
            return;

        ++_callbacks;

        if (result.underrun)
            ++_underruns;

        for (int channel = 0; channel < _output.channelCount(); ++channel)
        {
            for (const float sample : _output[channel])
                _checksum += static_cast<double>(sample) * sample;
        }
    }
} // namespace anasa