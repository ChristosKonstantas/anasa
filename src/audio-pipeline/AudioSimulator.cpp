#include "AudioSimulator.hpp"
#include "audio-pipeline/AudioConstants.hpp"

#include <cassert>
#include <chrono>
#include <thread>

namespace anasa 
{
    using Clock = std::chrono::steady_clock;
    AudioSimulator::AudioSimulator(AudioSettings settings, SharedState& sharedState, SpscQueue<AudioBlock>& readyAudioQueue)
        : _settings(settings),
          _sharedState(sharedState),
          _readyAudioQueue(readyAudioQueue),
          _stopRequested(false),
          _started(false),
          _callbacks(0),
          _underruns(0),
          _callbackMaxInUs(0),
          _checksum(0.0)
    {
        assert(_settings.sampleRate > 0);
        assert(_settings.audioBlockFrames > 0);
        assert(_settings.audioBlockFrames <= MAX_AUDIO_BLOCK_FRAMES);
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

        _started = true;

        _audioThread = std::thread(&AudioSimulator::periodicAudioDeviceClock, this);
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
        while (!_stopRequested.load(std::memory_order_acquire) && !_sharedState.stop.load(std::memory_order_acquire)) 
        {
            // The simulated audio thread has no block to consume yet, so it sleeps.
            // At nextCallbackTime, another audio block becomes due for consumption.
            std::this_thread::sleep_until(nextCallbackTime);

            // Shutdown may be requested while the audio thread is sleeping. 
            // Without this check, the thread would wake and execute one additional callback after stop() had been requested.
            if (_stopRequested.load(std::memory_order_acquire) || _sharedState.stop.load(std::memory_order_acquire))
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

                long long callbackDurationInUsecs = std::chrono::duration_cast<std::chrono::microseconds>
                (Clock::now() - callbackStart).count();

                _callbackMaxInUs = _callbackMaxInUs > callbackDurationInUsecs ? _callbackMaxInUs : callbackDurationInUsecs;


                // Advance virtual device clock from the previous scheduled deadline
                // NOTE: Clock::now() + period would accumulate timing drift whenever a wake-up was late ->
                // -> late wake-up should not permanently move all future deadlines
                nextCallbackTime += period;

            // catch-up missed callback periods
            } while(nextCallbackTime <= wakeTime && 
                    !_stopRequested.load(std::memory_order_acquire) &&
                    !_sharedState.stop.load(std::memory_order_acquire));
        }
    }

    void AudioSimulator::audioCallback() 
    {
        // Read the current stream generation
        const int globalGeneration = _sharedState.generation.load(std::memory_order_acquire);

        // A seek or live edit starts a new published audio stream
        if (_audioState.generation != globalGeneration) // detect a seek or edit
        {
            _audioState.generation = globalGeneration;
            _audioState.expectedBlockStartFrame = alignFrameToAudioBlock(_sharedState.targetFrame.load(std::memory_order_acquire), 
                                                                         _settings.audioBlockFrames);
        }

        const AudioBlock* head = nullptr;
        // Remove outdated blocks without locks. Work is bounded by queue capacity.
        for (std::size_t i = 0; i < _readyAudioQueue.capacity(); ++i) 
        {
            head = _readyAudioQueue.front();

            if (head == nullptr)
                break;
            
            const bool outdatedGeneration = head->generation < globalGeneration;
            const bool oldFrame = head->generation == globalGeneration && head->firstFrame < _audioState.expectedBlockStartFrame;
            const bool headNotOutdated = !outdatedGeneration && !oldFrame;
            
            if (headNotOutdated)
                break;

            _readyAudioQueue.pop();

            // pop() destroyed the object head pointed to
            head = nullptr;
        }
        // If playback is paused or the engine is rebuffering, the callback returns
        if (!_sharedState.playing.load(std::memory_order_acquire))
            return;

        ++_callbacks;

        const bool isExactBlock =
            head!=nullptr &&
            head->generation == globalGeneration &&
            head->firstFrame == _audioState.expectedBlockStartFrame &&
            head->frameCount == _settings.audioBlockFrames;
            
        if (!isExactBlock) 
        {
            // No correct block was ready for this callback period.
            // The defined failure policy is one -conceptual- block of silence.
            ++_underruns;
        }
        else
        {
            // IMPORTANT:
            // Use the queue-owned AudioBlock BEFORE pop().
            for (int i = 0; i < head->frameCount; ++i)
            {
                const float sample = head->samples[i];

                _checksum += static_cast<double>(sample) * sample;
            }

            // We are completely finished with head.
            // pop() destroys the AudioBlock and allows the producer
            // to reuse this queue slot.
            _readyAudioQueue.pop();

        }
        
        // Advance the audio timeline
        _audioState.expectedBlockStartFrame += _settings.audioBlockFrames;
        // Publish progress to the scheduler
        _sharedState.nextUnconsumedFrame.store(_audioState.expectedBlockStartFrame, std::memory_order_release);
    }
} // namespace anasa
