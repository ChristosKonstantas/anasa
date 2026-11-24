#include "render/Renderer.hpp"
#include "render/RenderConstants.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace anasa
{
    Renderer::Renderer(const IRenderKernel& kernel, const IChunkVersionReader& versionTable)
        : _kernel(kernel),
          _versionTable(versionTable),
          _renderCancellation(_versionTable)
    {}

    Renderer::Renderer(std::unique_ptr<IRenderKernel> kernel, const IChunkVersionReader& versionTable)
        : _ownedKernel(std::move(kernel)),
          _kernel(requireKernel(_ownedKernel)),
          _versionTable(versionTable),
          _renderCancellation(_versionTable)
    {}

    const IRenderKernel& Renderer::requireKernel(const std::unique_ptr<IRenderKernel>& kernel)
    {
        // reject a nullptr before it is dereferenced in the return statement
        if (!kernel)
            throw std::invalid_argument("Renderer kernel must not be null");

        return *kernel;
    }

    bool Renderer::renderTile(RenderJob& job, int tileIndex, const std::atomic<bool>& stopRequested) const
    {
        if (tileIndex < 0 || tileIndex >= job.layout.tilesPerChunk())
            throw std::out_of_range("tileIndex is outside the chunk");

        if (job.chunk < 0 || job.chunk >= _versionTable.count())
            throw std::out_of_range("chunk index is outside the timeline");

        if (job.samples.channelCount() <= 0 || job.samples.frameCount() != job.layout.chunkFrames())
            throw std::invalid_argument("RenderJob storage must contain complete channels of one chunk");

        if (_renderCancellation.shouldCancel(job, stopRequested))
        {
            job.cancel();
            return false;
        }

        const int tileFirstFrame = tileIndex * job.layout.tileFrames();
        const int tileLastFrame = std::min(tileFirstFrame + job.layout.tileFrames(), job.layout.chunkFrames());
        const int chunkFirstFrame = job.layout.firstFrameOfChunk(job.chunk);

        for (int frame = tileFirstFrame; frame < tileLastFrame; ++frame)
        {
            const int frameInsideTile = frame - tileFirstFrame;

            if (frameInsideTile % CANCELLATION_CHECK_FRAMES == 0)
            {
                if (_renderCancellation.shouldCancel(job, stopRequested))
                {
                    job.cancel();
                    return false;
                }
            }

            const int globalFrame = chunkFirstFrame + frame;
            for (int channel = 0; channel < job.samples.channelCount(); ++channel)
                job.samples[channel][frame] = _kernel.renderSample(channel, globalFrame, job.version);
        }

        // Content may change after the last periodic check.
        if (_renderCancellation.shouldCancel(job, stopRequested))
        {
            job.cancel();
            return false;
        }

        return true;
    }
} // namespace anasa