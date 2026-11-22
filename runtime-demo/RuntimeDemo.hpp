#pragma once

#include <array>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "engine/Engine.hpp"
#include "engine/EngineTypes.hpp"
#include "render/PhraseTileRenderer.hpp"
#include "scheduler/SchedulerTypes.hpp"

class RuntimeDemo
{
public:
    RuntimeDemo();
    int run(int argc, char** argv);

private:
    using Clock = std::chrono::steady_clock;

    enum class Mode { Synthetic, Neural, Both };

    enum class ScenarioType
    {
        SeekRecovery,
        ViewportPreparedSeek,
        EditRecovery
    };

    struct ScenarioSettings
    {
        ScenarioSettings();

        ScenarioType scenarioType;
        std::string_view name;
        std::string_view recoveryName;

        int trialsPerPolicy;
        int timelineSeconds;
        int workIterations;

        int backgroundPrimingMilliseconds;
        int viewportPreparationMilliseconds;
        int playbackMilliseconds;
        int timeoutMilliseconds;
    };

    struct TrialResult
    {
        TrialResult();

        double recoveryMilliseconds;
        long long callbacks;
        long long underruns;
        long long maximumCallbackMicroseconds;
        double setupMilliseconds;
        double recordingMilliseconds;
        anasa::PhraseRenderMetrics neural;
        std::string wavePath;
    };

    struct PolicyResult
    {
        PolicyResult();

        anasa::SchedulingPolicyType policy;
        std::vector<double> recoveryLatenciesMilliseconds;
        long long maximumCallbackMicroseconds;
        long long callbacks;
        long long underruns;
    };

    struct ComparisonResult
    {
        ScenarioSettings settings;
        PolicyResult fifo;
        PolicyResult priority;
    };

    std::array<ScenarioSettings, 3> makeScenarios();
    static void validateScenario(const ScenarioSettings& settings);
    static const char* policyName(anasa::SchedulingPolicyType policy);

    template <typename Predicate>
    static void waitUntil(Predicate&& predicate, std::chrono::milliseconds timeout, std::string_view description);

    static double percentile(std::vector<double> values, double probability);
    static double underrunPercentage(const PolicyResult& result);
    static void postRequired(anasa::Engine& engine, anasa::Command command);
    static double waitForGenerationRecovery(anasa::Engine& engine, const anasa::PlaybackSnapshot& previousState, Clock::time_point commandPostedAt,
                                            const ScenarioSettings& scenario, std::string_view description);

    TrialResult runTrial(anasa::SchedulingPolicyType policy, const ScenarioSettings& scenario, int trialNumber);
    void addTrial(PolicyResult& result, const ScenarioSettings& scenario, int trialNumber);
    ComparisonResult runComparison(const ScenarioSettings& scenario);
    void printResults(const ComparisonResult& comparison);

    static void printUsage();
    void configure(int argc, char** argv);
    void runSuite(bool neural);

    static int parseInteger(std::string_view text, int minimum, int maximum);
    static anasa::SingingRequest makeRequest(int transpose);
    static std::shared_ptr<const anasa::PreparedSingingTimeline> makeTimeline();

    Mode                                                  _mode;
    bool                                                  _neural;
    int                                                   _workIterations;
    int                                                   _threads;
    int                                                   _steps;
    int                                                   _trialsPerPolicy;

    std::filesystem::path                                 _voicebank;
    std::filesystem::path                                 _outputDirectory;
    std::shared_ptr<const anasa::PreparedSingingTimeline> _timeline;
};
