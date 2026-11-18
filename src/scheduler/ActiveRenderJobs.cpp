#include "scheduler/ActiveRenderJobs.hpp"

#include <stdexcept>

namespace anasa
{
    ActiveRenderJobs::ActiveRenderJobs(int channelCount, int chunkCount,
                                 RenderLayout layout)
    : _layout(layout),
      _channelCount(channelCount)
    {
        if (_channelCount <= 0)
            throw std::invalid_argument("channelCount must be greater than zero");

        if (chunkCount <= 0)
            throw std::invalid_argument("chunkCount must be greater than zero");

        _jobs.resize(static_cast<std::size_t>(chunkCount));
    }

    bool ActiveRenderJobs::hasCurrent(int chunk, int version) const noexcept
    {
        if (chunk < 0 || chunk >= static_cast<int>(_jobs.size()))
            return false;

        const std::shared_ptr<RenderJob>& job = _jobs[chunk];
        return job != nullptr && job->version == version && !job->isCancelled();
    }

    std::shared_ptr<RenderJob> ActiveRenderJobs::create(int chunk, int version)
    {
        if (chunk < 0 || chunk >= static_cast<int>(_jobs.size()))
            throw std::out_of_range("chunk index is outside the timeline");

        cancel(chunk);

        std::shared_ptr<RenderJob> job = std::make_shared<RenderJob>(_channelCount, _layout);
        
        job->chunk = chunk;
        job->version = version;
        _jobs[chunk] = job;

        return job;
    }

    void ActiveRenderJobs::cancel(int chunk) noexcept
    {
        if (chunk < 0 || chunk >= static_cast<int>(_jobs.size()))
            return;

        const std::shared_ptr<RenderJob>& job = _jobs[chunk];

        if (job != nullptr)
            job->cancel();
    }

    void ActiveRenderJobs::cancelAll() noexcept
    {
        for (std::shared_ptr<RenderJob>& job : _jobs)
        {
            if (job != nullptr)
            {
                job->cancel();
                job.reset();
            }
        }
    }

    bool ActiveRenderJobs::finish(const std::shared_ptr<RenderJob>& job) noexcept
    {
        if (job == nullptr || job->chunk < 0 || job->chunk >= static_cast<int>(_jobs.size()))
            return false;

        std::shared_ptr<RenderJob>& activeJob = _jobs[job->chunk];

        if (activeJob != job)
            return false;

        const bool usable = !job->isCancelled();
        
        activeJob.reset();
        
        return usable;
    }
} // namespace anasa
