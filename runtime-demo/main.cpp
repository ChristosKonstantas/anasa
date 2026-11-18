#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "engine/Engine.hpp"
#include "engine/EngineSettings.hpp"
#include "engine/EngineTypes.hpp"
#include "scheduler/SchedulerTypes.hpp"

namespace
{
    using Clock = std::chrono::steady_clock;

    enum class ScenarioType
    {
        SeekRecovery,
        ViewportPreparedSeek,
        EditRecovery
    };

    struct ScenarioSettings
    {
        ScenarioType     scenarioType = ScenarioType::SeekRecovery;
        std::string_view name;
        std::string_view recoveryName;

        int trialsPerPolicy = 5;
        int timelineSeconds = 12;
        int workIterations = 300;

        int backgroundPrimingMilliseconds = 100;
        int viewportPreparationMilliseconds = 0;
        int playbackMilliseconds = 2000;
        int timeoutMilliseconds = 10000;
    };

    struct TrialResult
    {
        double recoveryMilliseconds = 0.0;
        long long callbacks = 0;
        long long underruns = 0;
    };

    struct PolicyResult
    {
        anasa::SchedulingPolicyType policy = anasa::SchedulingPolicyType::Fifo;
        std::vector<double> recoveryLatenciesMilliseconds;
        long long callbacks = 0;
        long long underruns = 0;
    };

    struct ComparisonResult
    {
        ScenarioSettings settings;
        PolicyResult fifo;
        PolicyResult priority;
    };

    std::array<ScenarioSettings, 3> makeScenarios()
    {
        ScenarioSettings seek;

        seek.scenarioType = ScenarioType::SeekRecovery;
        seek.name = "Seek recovery under background backlog";
        seek.recoveryName = "Seek-to-play latency";
        seek.trialsPerPolicy = 5;
        seek.timelineSeconds = 12;
        seek.workIterations = 1000;
        seek.backgroundPrimingMilliseconds = 100;
        seek.viewportPreparationMilliseconds = 0;
        seek.playbackMilliseconds = 2000;
        seek.timeoutMilliseconds = 10000;

        ScenarioSettings viewport = seek;

        viewport.scenarioType = ScenarioType::ViewportPreparedSeek;
        viewport.name = "Viewport-guided seek preparation";
        viewport.recoveryName = "Seek-to-play after viewport preparation";
        viewport.viewportPreparationMilliseconds = 100;

        ScenarioSettings edit = seek;

        edit.scenarioType = ScenarioType::EditRecovery;
        edit.name = "Edit recovery during playback";
        edit.recoveryName = "Edit-to-play latency";

        return {seek, viewport, edit};
    }

    void validateScenario(const ScenarioSettings& settings)
    {
        if (settings.name.empty())
            throw std::invalid_argument("Scenario name must not be empty");

        if (settings.recoveryName.empty())
            throw std::invalid_argument("Recovery name must not be empty");

        if (settings.trialsPerPolicy <= 0)
            throw std::invalid_argument("trialsPerPolicy must be greater than zero");

        if (settings.timelineSeconds <= 0)
            throw std::invalid_argument("timelineSeconds must be greater than zero");

        if (settings.workIterations < 0)
            throw std::invalid_argument("workIterations must not be negative");

        if (settings.backgroundPrimingMilliseconds < 0)
            throw std::invalid_argument("backgroundPrimingMilliseconds must not be negative");

        if (settings.viewportPreparationMilliseconds < 0)
            throw std::invalid_argument("viewportPreparationMilliseconds must not be negative");

        if (settings.scenarioType == ScenarioType::ViewportPreparedSeek && settings.viewportPreparationMilliseconds == 0)
            throw std::invalid_argument("Viewport preparation scenario requires a positive preparation interval");

        if (settings.playbackMilliseconds <= 0)
            throw std::invalid_argument("playbackMilliseconds must be greater than zero");

        if (settings.timeoutMilliseconds <= 0)
            throw std::invalid_argument("timeoutMilliseconds must be greater than zero");
    }

    const char* policyName(anasa::SchedulingPolicyType policy)
    {
        return policy == anasa::SchedulingPolicyType::Fifo ? "FIFO" : "Priority";
    }

    template <typename Predicate>
    void waitUntil(Predicate&& predicate, std::chrono::milliseconds timeout, std::string_view description)
    {
        const Clock::time_point deadline = Clock::now() + timeout;

        while (!predicate())
        {
            if (Clock::now() >= deadline)
                throw std::runtime_error("Timed out waiting for " + std::string(description));

            std::this_thread::sleep_for(std::chrono::microseconds(250));
        }
    }

    double percentile(std::vector<double> values, double probability)
    {
        if (values.empty())
            throw std::invalid_argument("Cannot calculate a percentile of an empty sample");

        if (probability <= 0.0 || probability > 1.0)
            throw std::invalid_argument("Percentile probability must be in (0, 1]");

        std::sort(values.begin(), values.end());

        const std::size_t rank = static_cast<std::size_t>(std::ceil(probability * static_cast<double>(values.size())));
        const std::size_t index = std::clamp<std::size_t>(rank, 1, values.size()) - 1;

        return values[index];
    }

    double underrunPercentage(const PolicyResult& result)
    {
        if (result.callbacks == 0)
            return 0.0;

        return 100.0 * static_cast<double>(result.underruns) / static_cast<double>(result.callbacks);
    }

    void postRequired(anasa::Engine& engine, anasa::Command command)
    {
        if (!engine.post(command))
            throw std::runtime_error("Engine rejected a command");
    }

    double waitForGenerationRecovery(anasa::Engine& engine, const anasa::PlaybackSnapshot& previousState, Clock::time_point commandPostedAt,
                                     const ScenarioSettings& scenario, std::string_view description)
    {
        waitUntil(
            [&]
            {
                const anasa::PlaybackSnapshot state = engine.playbackSnapshot();

                const bool generationAdvanced = state.generation == previousState.generation + 1;
                const bool cursorAcknowledgedGeneration = state.cursorGeneration == state.generation;

                return generationAdvanced && cursorAcknowledgedGeneration && state.playing;
            },
            std::chrono::milliseconds(scenario.timeoutMilliseconds), description);

        return std::chrono::duration<double, std::milli>(Clock::now() - commandPostedAt).count();
    }

    TrialResult runTrial(anasa::SchedulingPolicyType policy, const ScenarioSettings& scenario)
    {
        anasa::EngineSettings engineSettings;

        engineSettings.timelineInSeconds = scenario.timelineSeconds;
        engineSettings.render.workIterations = scenario.workIterations;
        engineSettings.scheduler.policyType = policy;

        const int totalFrames = engineSettings.timelineInSeconds * engineSettings.audio.sampleRate;
        const int unalignedSeekTarget = totalFrames / 2;
        const int seekTarget = unalignedSeekTarget - unalignedSeekTarget % engineSettings.audio.audioBlockFrames;

        const int viewportFirstFrame = seekTarget;
        const int viewportLastFrame = std::min(totalFrames, seekTarget + 2 * engineSettings.audio.sampleRate);

        const int editFirstFrame = seekTarget + engineSettings.audio.sampleRate;
        const int editLastFrame = std::min(totalFrames - 1, editFirstFrame + engineSettings.audio.sampleRate / 4);

        const int playbackBeforeEditMilliseconds = scenario.playbackMilliseconds / 2;
        const int playbackAfterEditMilliseconds = scenario.playbackMilliseconds - playbackBeforeEditMilliseconds;

        anasa::Engine engine(engineSettings);

        bool engineStopped = false;

        auto stopEngine = [&]
        {
            if (engineStopped)
                return;

            engine.stop();
            engineStopped = true;
        };

        try
        {
            engine.start();

            std::this_thread::sleep_for(std::chrono::milliseconds(scenario.backgroundPrimingMilliseconds));

            if (scenario.scenarioType == ScenarioType::ViewportPreparedSeek)
            {
                postRequired(engine, {anasa::CommandType::SetViewport, viewportFirstFrame, viewportLastFrame});
                std::this_thread::sleep_for(std::chrono::milliseconds(scenario.viewportPreparationMilliseconds));
            }

            const anasa::PlaybackSnapshot beforeSeek = engine.playbackSnapshot();
            const Clock::time_point seekPostedAt = Clock::now();

            postRequired(engine, {anasa::CommandType::Seek, seekTarget, 0});
            postRequired(engine, {anasa::CommandType::Play, 0, 0});

            const double seekRecoveryMilliseconds = waitForGenerationRecovery(engine, beforeSeek, seekPostedAt, scenario, "seek recovery");

            TrialResult result;

            switch (scenario.scenarioType)
            {
                case ScenarioType::SeekRecovery:
                case ScenarioType::ViewportPreparedSeek:
                {
                    result.recoveryMilliseconds = seekRecoveryMilliseconds;

                    std::this_thread::sleep_for(std::chrono::milliseconds(scenario.playbackMilliseconds));

                    break;
                }

                case ScenarioType::EditRecovery:
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(playbackBeforeEditMilliseconds));

                    if (!engine.playbackSnapshot().playing)
                        throw std::runtime_error("Playback stopped before the edit");

                    const anasa::PlaybackSnapshot beforeEdit = engine.playbackSnapshot();
                    const Clock::time_point editPostedAt = Clock::now();

                    postRequired(engine, {anasa::CommandType::Edit, editFirstFrame, editLastFrame});

                    result.recoveryMilliseconds = waitForGenerationRecovery(engine, beforeEdit, editPostedAt, scenario, "edit recovery");

                    std::this_thread::sleep_for(std::chrono::milliseconds(playbackAfterEditMilliseconds));

                    break;
                }
            }

            if (!engine.playbackSnapshot().playing)
                throw std::runtime_error("Playback stopped before the observation window ended");

            stopEngine();

            const anasa::EngineMetrics metrics = engine.metrics();

            result.callbacks = metrics.callbacks;
            result.underruns = metrics.underruns;

            return result;
        }
        catch (...)
        {
            stopEngine();
            throw;
        }
    }

    void addTrial(PolicyResult& result, const ScenarioSettings& scenario, int trialNumber)
    {
        std::cout << "Running " << scenario.name << " | work=" << scenario.workIterations << " | "
                  << policyName(result.policy) << " | trial " << trialNumber << '/' << scenario.trialsPerPolicy
                  << " ... " << std::flush;

        const TrialResult trial = runTrial(result.policy, scenario);

        result.recoveryLatenciesMilliseconds.push_back(trial.recoveryMilliseconds);
        result.callbacks += trial.callbacks;
        result.underruns += trial.underruns;

        std::cout << std::fixed << std::setprecision(2)
                  << trial.recoveryMilliseconds << " ms, "
                  << trial.underruns << " underruns\n";
    }

    ComparisonResult runComparison(const ScenarioSettings& scenario)
    {
        validateScenario(scenario);

        ComparisonResult comparison;
        comparison.settings = scenario;
        comparison.fifo.policy = anasa::SchedulingPolicyType::Fifo;
        comparison.priority.policy = anasa::SchedulingPolicyType::Priority;

        comparison.fifo.recoveryLatenciesMilliseconds.reserve(static_cast<std::size_t>(scenario.trialsPerPolicy));
        comparison.priority.recoveryLatenciesMilliseconds.reserve(static_cast<std::size_t>(scenario.trialsPerPolicy));

        for (int trial = 1; trial <= scenario.trialsPerPolicy; ++trial)
        {
            const std::array<anasa::SchedulingPolicyType, 2> order = trial % 2 == 1 ? 
                                                                     std::array{anasa::SchedulingPolicyType::Fifo, anasa::SchedulingPolicyType::Priority}
                                                                   : std::array{anasa::SchedulingPolicyType::Priority, anasa::SchedulingPolicyType::Fifo};

            for (const anasa::SchedulingPolicyType policy : order)
            {
                PolicyResult& result = policy == anasa::SchedulingPolicyType::Fifo ? comparison.fifo : comparison.priority;
                addTrial(result, scenario, trial);
            }
        }

        return comparison;
    }

    void printResults(const ComparisonResult& comparison)
    {
        std::cout << "\n" << comparison.settings.name << "\n"
                  << "Recovery metric: " << comparison.settings.recoveryName
                  << " | Trials per policy: " << comparison.settings.trialsPerPolicy
                  << " | Work iterations: " << comparison.settings.workIterations
                  << " | Active playback: " << comparison.settings.playbackMilliseconds << " ms\n";

        if (comparison.settings.scenarioType == ScenarioType::ViewportPreparedSeek)
            std::cout << "Viewport preparation: " << comparison.settings.viewportPreparationMilliseconds << " ms\n";

        std::cout << '\n'
                  << std::left << std::setw(12) << "Policy"
                  << std::right << std::setw(18) << "Median recovery"
                  << std::setw(17) << "Worst recovery"
                  << std::setw(12) << "Underruns"
                  << std::setw(12) << "Callbacks"
                  << std::setw(16) << "Underrun rate" << '\n';

        for (const PolicyResult* result : {&comparison.fifo, &comparison.priority})
        {
            const double medianRecovery = percentile(result->recoveryLatenciesMilliseconds, 0.50);
            const double worstRecovery = *std::max_element(result->recoveryLatenciesMilliseconds.begin(), result->recoveryLatenciesMilliseconds.end());

            std::cout << std::left << std::setw(12) << policyName(result->policy)
                      << std::right << std::fixed << std::setprecision(2)
                      << std::setw(15) << medianRecovery << " ms"
                      << std::setw(14) << worstRecovery << " ms"
                      << std::setw(12) << result->underruns
                      << std::setw(12) << result->callbacks
                      << std::setw(15) << underrunPercentage(*result) << "%\n";
        }
    }
}

int main()
{
    try
    {
        const std::array<ScenarioSettings, 3> scenarios = makeScenarios();

        for (const ScenarioSettings& scenario : scenarios)
            printResults(runComparison(scenario));

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Comparison failed: " << exception.what() << '\n';
        return 1;
    }
}