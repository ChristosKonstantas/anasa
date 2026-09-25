#include "render/RenderCancellation.hpp"

namespace anasa
{
    RenderCancellation::RenderCancellation(const IChunkVersionReader& versionTable)
        : _versionTable(versionTable)
    {}

    bool RenderCancellation::shouldCancel(const RenderJob& job, const std::atomic<bool>& stopRequested) const
    {
        if (stopRequested.load(std::memory_order_acquire))
            return true;

        if (job.cancelled.load(std::memory_order_relaxed))
            return true;

        return _versionTable.get(job.chunk) != job.version;
    }
} // namespace anasa