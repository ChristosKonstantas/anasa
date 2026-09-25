#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <atomic>

#include "render/IChunkVersionReader.hpp"
#include "render/ITileRenderer.hpp"
#include "render/RenderCancellation.hpp"
#include "render/RenderTypes.hpp"
#include "render/kernels/IRenderKernel.hpp"

namespace anasa
{
    class Renderer final : public ITileRenderer
    {
    public:
        // The kernel and version reader must outlive this renderer and all calls to renderTile().
        Renderer(const IRenderKernel& kernel, const IChunkVersionReader& versionTable);

        // Validates and renders one tile with periodic cancellation checks.
        bool renderTile(RenderJob& job, int tileIndex, const std::atomic<bool>& stopRequested) const override;

    private:
        const IRenderKernel&       _kernel;
        const IChunkVersionReader& _versionTable;
        RenderCancellation         _renderCancellation;
    };
} // namespace anasa

#endif // RENDERER_HPP