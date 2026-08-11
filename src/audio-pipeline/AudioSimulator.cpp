#include "AudioSimulator.hpp"
#include <chrono>
#include <thread>

namespace anasa 
{
    using Clock = std::chrono::steady_clock;
    AudioSimulator::AudioSimulator(SharedState& sharedState, SpscQueue<AudioBlock, READY_QUEUE_SLOTS>& readyAudioQueue)
        : _sharedState(sharedState),
          _readyAudioQueue(readyAudioQueue),
          _stopRequested(false),
          _started(false),
          _callbacks(0),
          _underruns(0),
          _callbackMaxInUs(0),
          _checksum(0.0)
    {
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
        (std::chrono::duration<double>(static_cast<double>(AUDIO_BLOCK_FRAMES) / static_cast<double>(SAMPLE_RATE)));

        // If the engine starts now, the first callback is scheduled for now + period
        Clock::time_point nextCallbackTime = Clock::now() + period;
        
        // Audio thread continues until another thread requests shutdown
        while (!_stopRequested.load(std::memory_order_acquire) &&
               !_sharedState.stop.load(std::memory_order_acquire)) 
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

                _callbackMaxInUs = _callbackMaxInUs > callbackDurationInUsecs? _callbackMaxInUs : callbackDurationInUsecs;


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
        int globalGeneration = _sharedState.generation.load(std::memory_order_acquire);

        // A seek or live edit starts a new published audio stream
        if (_audioState.generation != globalGeneration) // detect a seek or edit
        {
            _audioState.generation = globalGeneration;
            _audioState.expectedBlockStartFrame = alignToAudioBlock(_sharedState.targetFrame.load(std::memory_order_acquire));
        }

        AudioBlock head; // Temporary object for examining the spsc queue head
        // Remove outdated blocks without locks. Work is bounded by queue capacity.
        for (int i = 0; i < _readyAudioQueue.usableCapacity(); ++i) 
        {
            // peek() copies the head without removing it
            if (!_readyAudioQueue.peek(head))
                break;
            
            bool outdatedGeneration = head.generation < globalGeneration;
            bool oldFrame = head.generation == globalGeneration && head.firstFrame < _audioState.expectedBlockStartFrame;
            bool headNotOutdated = !outdatedGeneration && !oldFrame;
            
            if (headNotOutdated)
                break;

            _readyAudioQueue.pop(head);
        }
        // If playback is paused or the engine is rebuffering, the callback returns
        if (!_sharedState.playing.load(std::memory_order_acquire))
            return;

        ++_callbacks;

        bool isExactBlock = _readyAudioQueue.peek(head) && head.generation == globalGeneration && head.firstFrame == _audioState.expectedBlockStartFrame;

        if (!isExactBlock) 
        {
            // No correct block was ready for this callback period.
            // The defined failure policy is one -conceptual- block of silence.
            ++_underruns;
        }
        else
        {
            _readyAudioQueue.pop(head); // consume the exact block and remove from the queue
            // The project has no real audio device so it can't send the samples to speakers
            // therefore we use _checksum to show that we consume audio data
            // and prevent compiler from treating the rendered samples as entirely unused
            for (float sample : head.samples)
                _checksum += static_cast<double>(sample) * sample;
        }
        
        // Advance the audio timeline
        _audioState.expectedBlockStartFrame += AUDIO_BLOCK_FRAMES;
        // Publish progress to the scheduler
        _sharedState.nextUnconsumedFrame.store(_audioState.expectedBlockStartFrame, std::memory_order_release);
    }

} // namespace anasa