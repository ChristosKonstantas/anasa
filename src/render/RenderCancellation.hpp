#ifndef RENDER_CANCELLATION_HPP
#define RENDER_CANCELLATION_HPP

#include <atomic>

#include "render/IChunkVersionReader.hpp"
#include "render/RenderTypes.hpp"

namespace anasa
{
    class RenderCancellation
    {
    public:
        explicit RenderCancellation(const IChunkVersionReader& versionTable);

        // The caller validates job.chunk before checking cancellation.
        bool shouldCancel(const RenderJob& job, const std::atomic<bool>& stopRequested) const;

    private:
        const IChunkVersionReader& _versionTable;
    };
} // namespace anasa

#endif // RENDER_CANCELLATION_HPP