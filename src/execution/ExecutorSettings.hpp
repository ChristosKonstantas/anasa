#ifndef EXECUTOR_SETTINGS_HPP
#define EXECUTOR_SETTINGS_HPP

namespace anasa
{

struct ExecutorSettings
{
    int workerCount = 4;
    // Maximum number of tasks waiting inside the Executor.
    // Tasks already running on workers are not included.
    int queuedTaskCapacity = 8;
};

} // namespace anasa

#endif // EXECUTOR_SETTINGS_HPP