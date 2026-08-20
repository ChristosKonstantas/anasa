#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <atomic>

#include "render/RenderSettings.hpp"
#include "render/RenderTypes.hpp"
#include "render/VersionTable.hpp"

namespace anasa
{

class Renderer
{
public:
    Renderer(int sampleRate, const RenderSettings& renderSettings, VersionTable& versionTable);

    /* ----> renderTile(...)
    * Synchronously renders one tile and writes it into the corresponding range of job.samples.
    *
    * The harmonic oscillator generates deterministic synthetic audio.
    * The configurable nonlinear loop adds synthetic serialized CPU load
    * It is not a sophisticated DSP or neural-inference algorithm.
    *
    * Returns false when shutdown, explicit cancellation, or version invalidation is detected.
    */
    bool          renderTile(RenderJob& job, int tileIndex, const std::atomic<bool>& stopRequested) const;

private:
    bool          shouldCancel(const RenderJob& job, const std::atomic<bool>& stopRequested) const;

    int           _sampleRate;
    int           _workIterations;

    VersionTable& _versionTable;
};

} // namespace anasa

#endif // RENDERER_HPP