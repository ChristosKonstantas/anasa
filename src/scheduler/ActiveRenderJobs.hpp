#ifndef ACTIVE_RENDER_JOBS_HPP
#define ACTIVE_RENDER_JOBS_HPP

#include <memory>
#include <vector>

#include "render/RenderLayout.hpp"
#include "render/RenderTypes.hpp"

namespace anasa
{
    class ActiveRenderJobs
    {
    public:
        ActiveRenderJobs(int channelCount, int chunkCount, RenderLayout layout = RenderLayout());

        ActiveRenderJobs(const ActiveRenderJobs&) = delete;
        ActiveRenderJobs& operator=(const ActiveRenderJobs&) = delete;
        ActiveRenderJobs(ActiveRenderJobs&&) = delete;
        ActiveRenderJobs& operator=(ActiveRenderJobs&&) = delete;

        bool hasCurrent(int chunk, int version) const noexcept;

        std::shared_ptr<RenderJob> create(int chunk, int version);

        void cancel(int chunk) noexcept;
        void cancelAll() noexcept;

        bool finish(const std::shared_ptr<RenderJob>& job) noexcept;

    private:
        const RenderLayout                      _layout;
        const int                               _channelCount;
        std::vector<std::shared_ptr<RenderJob>> _jobs;
    };
} // namespace anasa

#endif // ACTIVE_RENDER_JOBS_HPP
