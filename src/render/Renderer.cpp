#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "render/Renderer.hpp"
#include "render/RenderConstants.hpp"

namespace anasa
{
constexpr float PI = 3.14159265358979323846f;

Renderer::Renderer(int sampleRate, const RenderSettings& renderSettings, VersionTable& versionTable)
    : _sampleRate(sampleRate),
      _workIterations(renderSettings.workIterations),
      _versionTable(versionTable)
{
    if(_sampleRate <= 0)
        throw std::invalid_argument("Sample rate should always be positive");
    if(_workIterations < 0)
        throw std::invalid_argument("Work iterations can not be negative");
}

bool Renderer::renderTile(RenderJob& job, int tileIndex, const std::atomic<bool>& stopRequested) const
{
    if (tileIndex < 0 || tileIndex >= TILES_PER_CHUNK)
        throw std::out_of_range("tileIndex is outside the chunk");

    if (job.chunk < 0 || job.chunk >= _versionTable.count())
        throw std::out_of_range("chunk index is outside the timeline");

    if (shouldCancel(job, stopRequested))
    {
        job.cancelled.store(true, std::memory_order_release);

        return false;
    }

    // tileIndex inside a chunk is a relative position
    
    // if tileIndex = 2
    const int tileFirstFrame = tileIndex * TILE_FRAMES; // = 2 * 256 = 512

    const int tileLastFrame = std::min(tileFirstFrame + TILE_FRAMES, CHUNK_FRAMES); // = 512 + 256 = 768 

    // if job.chunk = 3
    const int chunkFirstFrame = job.chunk * CHUNK_FRAMES; // = 3 * 2048 = 6144

    // A different content version produces different audio (toy-only behavior).
    const float revision = static_cast<float>(job.version % 13) / 13.0f;
    
    // Synthetic serialized CPU workload used to stress the scheduler. This is not a sophisticated DSP or inference algorithm.
    for (int frame = tileFirstFrame; frame < tileLastFrame; ++frame) // 512 ... 767
    {
        int frameInsideTile = frame - tileFirstFrame; // 0 ... 255

        // Avoid checking atomics for every sample while still allowing a long render to stop reasonably quickly using a periodic check
        if (frameInsideTile % CANCELLATION_CHECK_FRAMES == 0)
        {
            if (shouldCancel(job, stopRequested))
            {
                job.cancelled.store(true, std::memory_order_release);

                return false;
            }
        }

        const int globalFrame = chunkFirstFrame + frame; // 6144 + 512 = 6656 ... 6144 + 767 = 6911

        const float seconds = static_cast<float>(globalFrame) / static_cast<float>(_sampleRate); // ~ 138.67 ms ... 143.979 ms 
        
        // the sample represents the waveform at approximately 138.67 ms on the project timeline

        /* Generate a deterministic voiced signal from the absolute timeline position */ 
        
        // Different content versions produce a slightly different fundamental frequency.
        const float baseFrequency = 110.0f + 14.0f * revision; // f = 110 .. 122.922 Hz - repeats every 13 versions -
        
        // Calculate oscillator phase
        const float phase = 2.0f * PI * baseFrequency * seconds; // 2 * pi * f * t

        // Fundamental frequency.
        float voice = 0.18f * std::sin(phase); // 1 * f

        // Second harmonic.
        voice +=  0.08f * std::sin(2.0f * phase); // 2 * f

        // Third harmonic.
        voice +=  0.04f * std::sin(3.0f * phase); // 3 * f
        
        // Artificial nonlinear workload standing in for "neural inference" or an expensive DSP kernel.
        float state = voice + revision;

        for (int iteration = 0; iteration < _workIterations; ++iteration)
        {
            const float drive = 0.001f * static_cast<float>((iteration % 17) - 8); // -0.008 - 0.008

            state = std::tanh(0.985f * state + drive + 0.01f * revision);
        }

        job.samples[frame] = std::tanh(voice * (0.8f + 0.2f * state));
    }

    // The version could change after the final periodic check.
    if (shouldCancel(job, stopRequested))
    {
        job.cancelled.store(true, std::memory_order_release);

        return false;
    }

    return true;
}

bool Renderer::shouldCancel(const RenderJob& job, const std::atomic<bool>& stopRequested) const
{
    if (stopRequested.load(std::memory_order_acquire)) // engine/executor shutdown
    {
        return true;
    }

    if (job.cancelled.load(std::memory_order_acquire)) // this job has already been marked as cancelled.
    {
        return true;
    }

    return _versionTable.get(job.chunk) != job.version; // an edit has made the job obsolete.
}

} // namespace anasa