#ifndef FUNCTIONS_HPP
#define FUNCTIONS_HPP

#include <chrono>
#include "utils/queues/SpscQueue.hpp"
#include "render/RenderConstants.hpp"
#include "render/RenderSettings.hpp"
#include "render/RenderTypes.hpp"
#include "render/VersionTable.hpp"
#include "render/Renderer.hpp"
#include "execution/ExecutorConstants.hpp"
#include "execution/ExecutorSettings.hpp"
#include "execution/ExecutorTypes.hpp"
#include "execution/Executor.hpp"
#include "scheduler/SchedulerTypes.hpp"
#include "audio-pipeline/AudioTypes.hpp"

namespace anasa::functions
{
    using namespace std::chrono_literals;
    constexpr float UNTOUCHED_SAMPLE = 0.12345f;
    
    struct BlockHeader
    {
        int generation = 0;
        int firstFrame = 0;
        int frameCount = 0;
    };

    inline ExecutorSettings makeTestExecutorSettings(int workerCount = 4, int queuedTaskCapacity = 8)
    {
        ExecutorSettings settings;
        settings.workerCount = workerCount;
        settings.queuedTaskCapacity = queuedTaskCapacity;

        return settings;
    }

    inline RenderSettings makeTestRenderSettings(int workIterations = 4)
    {
        RenderSettings settings;
        settings.workIterations = workIterations;

        return settings;
    }

    inline std::shared_ptr<RenderJob> makeTestRenderJob(VersionTable& versions, int chunk)
    {
        std::shared_ptr<RenderJob> job = std::make_shared<RenderJob>();

        job->chunk = chunk;
        job->version = versions.get(chunk);
        job->tilesRemaining.store(TILES_PER_CHUNK, std::memory_order_release);
        job->cancelled.store(false, std::memory_order_release);

        std::fill(job->samples.begin(), job->samples.end(), UNTOUCHED_SAMPLE);

        return job;
    }

    inline void initializeJob(RenderJob& job, VersionTable& versions, int chunk)
    {
        job.chunk = chunk;
        job.version = versions.get(chunk);
        job.cancelled.store(false, std::memory_order_release);
        job.samples.fill(UNTOUCHED_SAMPLE);
    }

    inline bool waitForCompletedJob(Executor& executor, std::shared_ptr<RenderJob>& completedJob, std::chrono::milliseconds timeout = 2000ms)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        while (std::chrono::steady_clock::now() < deadline)
        {
            if (executor.popCompleted(completedJob))
                return true;

            std::this_thread::sleep_for(1ms);
        }

        return false;
    }
     
    template <typename Predicate>
    inline bool waitUntil(Predicate&& predicate, std::chrono::milliseconds timeout = 3000ms)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        while (std::chrono::steady_clock::now() < deadline)
        {
            if (predicate())
                return true;

            std::this_thread::sleep_for(1ms);
        }

        return predicate();
    }

    inline bool waitAndPopBlock(SpscQueue<AudioBlock>& queue,
                            BlockHeader& result,
                            std::chrono::milliseconds timeout = 3000ms)
    {
        return waitUntil([&]
        {
            const AudioBlock* block = queue.front();

            if (block == nullptr)
                return false;

            result.generation = block->generation;
            result.firstFrame = block->firstFrame;
            result.frameCount = block->frameCount;

            queue.pop();
            return true;
        }, timeout);
    }
    
    inline bool waitAndPopFirstBlockOfGeneration(SpscQueue<AudioBlock>& queue, int generation, BlockHeader& result, 
                                            std::chrono::milliseconds timeout = 5000ms)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        while (std::chrono::steady_clock::now() < deadline)
        {
            const AudioBlock* block = queue.front();

            if (block == nullptr)
            {
                std::this_thread::sleep_for(1ms);
                continue;
            }

            const BlockHeader candidate{block->generation, block->firstFrame, block->frameCount};
            queue.pop();

            if (candidate.generation == generation)
            {
                result = candidate;
                return true;
            }
        }

        return false;
    }

    inline PendingRenderTile makeTile(RenderPriority priority, long long sequence, int deadlineFrame = -1, int distanceInFrames = 0)
    {
        PendingRenderTile tile;
        tile.priority = priority;
        tile.sequence = sequence;
        tile.deadlineFrame = deadlineFrame;
        tile.distanceInFrames = distanceInFrames;
        return tile;
    }

} // namespace anasa::functions

#endif // FUNCTIONS_HPP