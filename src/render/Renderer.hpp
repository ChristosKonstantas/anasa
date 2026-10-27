#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <atomic>
#include <memory>

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
        Renderer(const IRenderKernel& kernel, const IChunkVersionReader& versionTable);

        // Takes ownership of a non-null kernel.
        Renderer(std::unique_ptr<IRenderKernel> kernel, const IChunkVersionReader& versionTable);

        // Validates and renders one tile with periodic cancellation checks.
        bool renderTile(RenderJob& job, int tileIndex, const std::atomic<bool>& stopRequested) const override;

    private:
        static const IRenderKernel& requireKernel(const std::unique_ptr<IRenderKernel>& kernel);

        std::unique_ptr<IRenderKernel> _ownedKernel;
        const IRenderKernel&           _kernel;
        const IChunkVersionReader&     _versionTable;
        RenderCancellation             _renderCancellation;
    };
} // namespace anasa

#endif // RENDERER_HPP