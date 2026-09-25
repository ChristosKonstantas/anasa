#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <atomic>

#include "render/IChunkVersionReader.hpp"
#include "render/ITileRenderer.hpp"
#include "render/RenderSettings.hpp"
#include "render/RenderTypes.hpp"

namespace anasa
{

    class Renderer final : public ITileRenderer
    {
    public:
        // The version reader must outlive this renderer and all calls to renderTile().
        Renderer(int sampleRate, const RenderSettings& renderSettings, const IChunkVersionReader& versionTable);

        /* ----> renderTile(...)
        * Synchronously renders one tile and writes it into the corresponding range of job.samples.
        *
        * The harmonic oscillator generates deterministic synthetic audio.
        * The configurable nonlinear loop adds synthetic serialized CPU load
        * It is not a sophisticated DSP or neural-inference algorithm.
        *
        * Returns false when shutdown, explicit cancellation, or version invalidation is detected.
        */
        bool                       renderTile(RenderJob& job, int tileIndex, const std::atomic<bool>& stopRequested) const override;

    private:
        bool                       shouldCancel(const RenderJob& job, const std::atomic<bool>& stopRequested) const;

        int                        _sampleRate;
        int                        _workIterations;

        const IChunkVersionReader& _versionTable;
    };

} // namespace anasa

#endif // RENDERER_HPP