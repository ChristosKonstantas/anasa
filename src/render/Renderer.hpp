#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <atomic>

#include "render/IChunkVersionReader.hpp"
#include "render/ITileRenderer.hpp"
#include "render/RenderCancellation.hpp"
#include "render/RenderSettings.hpp"
#include "render/RenderTypes.hpp"
#include "render/kernels/SyntheticRenderKernel.hpp"

namespace anasa
{
    class Renderer final : public ITileRenderer
    {
    public:
        // The version reader must outlive this renderer and all calls to renderTile().
        Renderer(int sampleRate, const RenderSettings& renderSettings, const IChunkVersionReader& versionTable);

        // Validates and renders one synthetic tile with periodic cancellation checks.
        bool                       renderTile(RenderJob& job, int tileIndex, const std::atomic<bool>& stopRequested) const override;

    private:
        SyntheticRenderKernel      _kernel;
        const IChunkVersionReader& _versionTable;
        RenderCancellation         _renderCancellation;
    };
} // namespace anasa

#endif // RENDERER_HPP