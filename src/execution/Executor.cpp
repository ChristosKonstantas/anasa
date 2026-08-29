#include <cassert>
#include <stdexcept>
#include <utility>

#include "execution/Executor.hpp"
#include "render/RenderConstants.hpp"

namespace anasa
{

    Executor::Executor(const ExecutorSettings& settings, Renderer& renderer)
        : _settings(settings),
          _renderer(renderer),
          _stopRequested(false),
          _started(false)
    {
        if (_settings.workerCount <= 0)
            throw std::invalid_argument("workerCount must be greater than zero");

        if (_settings.queuedTaskCapacity <= 0)
            throw std::invalid_argument("queuedTaskCapacity must be greater than zero");
    }

    Executor::~Executor()
    {
        stop();
    }

    void Executor::start()
    {
        {
            std::lock_guard<std::mutex> lock(_taskMutex);

            if (_started)
                return;

            _stopRequested.store(false, std::memory_order_release);
            _started = true;
        }

        // Starting a new execution session discards completions from an older session.
        {
            std::lock_guard<std::mutex> lock(_completedMutex);

            while (!_completedJobs.empty())
                _completedJobs.pop();
        }

        try
        {
            _workers.reserve(_settings.workerCount);

            for (int worker = 0; worker < _settings.workerCount; ++worker)
                _workers.emplace_back([this]{workerLoop();});
        }
        catch (...)
        {
            stop();
            throw;
        }
    }

    void Executor::stop()
    {
        {
            std::lock_guard<std::mutex> lock(_taskMutex);

            if (!_started)
                return;

            _stopRequested.store(true, std::memory_order_release);

            // Any remaining work should not be executed during shutdown: cancel and remove every task that has not started rendering.
            while (!_queuedTasks.empty())
            {
                RenderTask& task = _queuedTasks.front();

                if (task.job)
                    task.job->cancelled.store(true, std::memory_order_release);

                _queuedTasks.pop();
            }
        }

        // Every sleeping worker must wake, observe stopRequested and exit.
        _taskConditionVariable.notify_all();

        for (std::thread& worker : _workers)
        {
            if (worker.joinable())
                worker.join();
        }

        _workers.clear();

        {
            std::lock_guard<std::mutex> lock(_taskMutex);
            _started = false;
        }
    }

    bool Executor::submit(RenderTask task)
    {
        if (!task.job)
            throw std::invalid_argument("RenderTask job must not be null");

        if (task.tileIndex < 0 || task.tileIndex >= TILES_PER_CHUNK)
            throw std::out_of_range("RenderTask tileIndex is outside the chunk");
        
        // (1) Enqueue a task
        {
            std::lock_guard<std::mutex> lock(_taskMutex);

            if (!_started || _stopRequested.load(std::memory_order_acquire))
                return false;

            if (static_cast<int>(_queuedTasks.size()) >= _settings.queuedTaskCapacity)
                return false;

            _queuedTasks.push(std::move(task));
        }

        _taskConditionVariable.notify_one(); // notify one thread waiting for this condition variable

        return true;
    }

    bool Executor::popCompleted(std::shared_ptr<RenderJob>& job)
    {
        std::lock_guard<std::mutex> lock(_completedMutex);

        if (_completedJobs.empty())
            return false;

        job = std::move(_completedJobs.front());
        _completedJobs.pop();

        return true;
    }

    int Executor::workerCount() const
    {
        return _settings.workerCount;
    }

    int Executor::queuedTaskCount()
    {
        std::lock_guard<std::mutex> lock(_taskMutex);

        return static_cast<int>(_queuedTasks.size());
    }

    void Executor::workerLoop()
    {
        while (true)
        {
            RenderTask task;
            // (1) With FIFO priority pop a task from the _queuedTasks
            {
                std::unique_lock<std::mutex> lock(_taskMutex);

                _taskConditionVariable.wait(lock, [this]{return _stopRequested.load(std::memory_order_acquire) || !_queuedTasks.empty();});

                if (_stopRequested.load(std::memory_order_acquire))
                    return;

                task = std::move(_queuedTasks.front());
                _queuedTasks.pop();
            }

            // (2) Render tile of popped task's job
            // _taskMutex is no longer held, so other workers can take tasks while this worker performs expensive rendering.
            try
            {
                bool rendered = _renderer.renderTile(*task.job, task.tileIndex, _stopRequested);

                if (!rendered)
                    task.job->cancelled.store(true, std::memory_order_release);
            }
            catch (...)
            {
                // An exception must never escape a worker thread.
                task.job->cancelled.store(true, std::memory_order_release);
            }
            
            // (3) Publish completed jobs when none of them exists anymore

            // previousTilesRemaining has the value before subtraction takes place
            int previousTilesRemaining = task.job->tilesRemaining.fetch_sub(1, std::memory_order_acq_rel);

            assert(previousTilesRemaining > 0);

            if (previousTilesRemaining == 1)
            {
                std::lock_guard<std::mutex> lock(_completedMutex);
                _completedJobs.push(std::move(task.job));
            }   
        }
    }

} // namespace anasa