#ifndef EXECUTOR_HPP
#define EXECUTOR_HPP

#include <atomic>
#include <condition_variable>
#include <queue>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "execution/IRenderExecutor.hpp"
#include "execution/ExecutorSettings.hpp"
#include "execution/ExecutorTypes.hpp"
#include "render/ITileRenderer.hpp"

namespace anasa
{
    /*---------------------------------------------------------------------------*/
    /*                       class Executor                                      */
    /*---------------------------------------------------------------------------*/
    /* Executes tile-rendering tasks on a fixed worker pool.                     */
    /*                                                                           */
    /* Threading contract:                                                       */
    /*  - Engine/control thread calls start() and stop().                        */
    /*  - Scheduler calls submit() and popCompleted().                           */
    /*  - Worker threads call workerLoop() and publishCompleted().               */
    /*  - The audio callback *never* accesses this class.                        */
    /*                                                                           */   
    /* start() and stop() can be repeated multiple times (idempotent) but are    */
    /* not intended to be called concurrently from different threads.            */
    /*---------------------------------------------------------------------------*/

    class Executor final : public IRenderExecutor
    {
    public:
        // The renderer must outlive the executor, including every worker thread.
        Executor(const ExecutorSettings& settings, const ITileRenderer& renderer);
        ~Executor() override;

        void                                   start(); // Starts the fixed worker pool. Repeated calls have no effect.
        void                                   stop();  // Cancel queued work, request cancellation of running work and joins every worker.
        bool                                   submit(RenderTask task) override; // Attempts to add one tile task to the bounded FIFO execution queue.
        bool                                   popCompleted(std::shared_ptr<RenderJob>& job) override; // Collect completed jobs. Moves one fully completed RenderJob to the caller.
        int                                    workerCount() const override; // Returns the fixed number of configured worker threads.
        int                                    queuedTaskCount() override; // Executor queue inspection providing task count
    
    private:
        void                                   workerLoop(); // Waits for work, removes one FIFO task, renders outside the queue mutex and exits when shutdown is requested.
        
        ExecutorSettings                       _settings;
        const ITileRenderer&                   _renderer;
        std::vector<std::thread>               _workers;

        std::queue<RenderTask>                 _renderTasksQueue; // 1 Scheduler (producer) / Many workers (consumers) -> SPMC
        std::mutex                             _taskMutex;
        std::condition_variable                _taskConditionVariable;

        std::queue<std::shared_ptr<RenderJob>> _completedJobsQueue; // Many workers (producers) / 1 Scheduler (consumer) -> MPSC
        std::mutex                             _completedMutex;

        std::atomic<bool>                      _stopRequested;
        bool                                   _started;
    };

} // namespace anasa

#endif // EXECUTOR_HPP