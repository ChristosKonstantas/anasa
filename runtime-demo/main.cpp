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
#include <utility>
#include <charconv>
#include <filesystem>
#include <memory>

#include "render/PhraseTileRenderer.hpp"
#include "../apps/WaveWriter.hpp"

#ifdef ANASA_OPENVPI
#include "render/openvpi/OpenVpiRenderer.hpp"
#endif

#include "engine/Engine.hpp"
#include "engine/EngineSettings.hpp"
#include "engine/EngineTypes.hpp"
#include "scheduler/SchedulerTypes.hpp"

class RuntimeDemo
{
public:
    int run(int argc, char** argv)
    {
        if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help"))
        {
            printUsage();
            return 0;
        }

        configure(argc, argv);

        if (_mode != Mode::Neural)
            runSuite(false);

        if (_mode != Mode::Synthetic)
            runSuite(true);

        return 0;
    }

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
        long long maximumCallbackMicroseconds = 0;
        double setupMilliseconds = 0.0;
        double recordingMilliseconds = 0.0;
        anasa::PhraseRenderMetrics neural;
        std::string wavePath;
    };

    struct PolicyResult
    {
        anasa::SchedulingPolicyType policy = anasa::SchedulingPolicyType::Fifo;
        std::vector<double> recoveryLatenciesMilliseconds;
        long long maximumCallbackMicroseconds = 0;
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
        seek.recoveryName = "Seek-to-ready latency";
        seek.trialsPerPolicy = _trialsPerPolicy;
        seek.timelineSeconds = 12;
        seek.workIterations = _workIterations;
        seek.backgroundPrimingMilliseconds = 100;
        seek.viewportPreparationMilliseconds = 0;
        seek.playbackMilliseconds = 2000;
        seek.timeoutMilliseconds = _neural ? 30000 : 10000;

        ScenarioSettings viewport = seek;

        viewport.scenarioType = ScenarioType::ViewportPreparedSeek;
        viewport.name = "Viewport-guided seek preparation";
        viewport.recoveryName = "Seek-to-ready after viewport preparation";
        viewport.viewportPreparationMilliseconds = 100;

        ScenarioSettings edit = seek;

        edit.scenarioType = ScenarioType::EditRecovery;
        edit.name = "Edit recovery during playback";
        edit.recoveryName = "Edit-to-ready latency";

        return {seek, viewport, edit};
    }

    static void validateScenario(const ScenarioSettings& settings)
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

    static const char* policyName(anasa::SchedulingPolicyType policy)
    {
        return policy == anasa::SchedulingPolicyType::Fifo ? "FIFO" : "Priority";
    }

    template <typename Predicate>
    static void waitUntil(Predicate&& predicate, std::chrono::milliseconds timeout, std::string_view description)
    {
        const Clock::time_point deadline = Clock::now() + timeout;

        while (!predicate())
        {
            if (Clock::now() >= deadline)
                throw std::runtime_error("Timed out waiting for " + std::string(description));

            std::this_thread::sleep_for(std::chrono::microseconds(250));
        }
    }

    static double percentile(std::vector<double> values, double probability)
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

    static double underrunPercentage(const PolicyResult& result)
    {
        if (result.callbacks == 0)
            return 0.0;

        return 100.0 * static_cast<double>(result.underruns) / static_cast<double>(result.callbacks);
    }

    static void postRequired(anasa::Engine& engine, anasa::Command command)
    {
        if (!engine.post(command))
            throw std::runtime_error("Engine rejected a command");
    }

    static double waitForGenerationRecovery(anasa::Engine& engine, const anasa::PlaybackSnapshot& previousState, Clock::time_point commandPostedAt,
                                            const ScenarioSettings& scenario, std::string_view description)
    {
        waitUntil(
            [&]
            {
                engine.rethrowRenderFailure();
                const anasa::PlaybackSnapshot state = engine.playbackSnapshot();

                const bool generationAdvanced = state.generation == previousState.generation + 1;
                const bool cursorAcknowledgedGeneration = state.cursorGeneration == state.generation;

                return generationAdvanced && cursorAcknowledgedGeneration && state.playing;
            },
            std::chrono::milliseconds(scenario.timeoutMilliseconds), description);

        return std::chrono::duration<double, std::milli>(Clock::now() - commandPostedAt).count();
    }

    TrialResult runTrial(anasa::SchedulingPolicyType policy, const ScenarioSettings& scenario, int trialNumber)
    {
        anasa::EngineSettings engineSettings;

        engineSettings.timelineInSeconds = scenario.timelineSeconds;
        engineSettings.render.workIterations = scenario.workIterations;
        engineSettings.scheduler.policyType = policy;

        if (_neural)
        {
            engineSettings.timelineFrames = _timeline->totalFrames();
            engineSettings.audio.sampleRate = _timeline->format().sampleRate;
            engineSettings.audio.channelCount = _timeline->format().channelCount;
            engineSettings.render.layout = anasa::RenderLayout(_timeline->phraseFrames(), _timeline->phraseFrames());
            engineSettings.render.contextFrames = 0;
            engineSettings.render.workIterations = 0;
            engineSettings.executor.workerCount = 1;
            engineSettings.executor.renderTasksQueueCapacity = 1;
            engineSettings.scheduler.urgentChunks = 2;
            engineSettings.scheduler.maxPendingTiles = 6;
            engineSettings.scheduler.urgentReservedTiles = 2;
        }

        engineSettings.captureFrames = 120 * engineSettings.audio.sampleRate;

        const int totalFrames = engineSettings.timelineFrames > 0 ? engineSettings.timelineFrames : engineSettings.timelineInSeconds * engineSettings.audio.sampleRate;
        const int unalignedSeekTarget = totalFrames / 2;
        const int seekTarget = unalignedSeekTarget - unalignedSeekTarget % engineSettings.audio.audioBlockFrames;

        const int viewportFirstFrame = seekTarget;
        const int viewportLastFrame = std::min(totalFrames, seekTarget + 2 * engineSettings.audio.sampleRate);

        const int editFirstFrame = seekTarget + engineSettings.audio.sampleRate;
        const int editLastFrame = std::min(totalFrames - 1, editFirstFrame + engineSettings.audio.sampleRate / 4);

        const int playbackBeforeEditMilliseconds = scenario.playbackMilliseconds / 2;
        const int playbackAfterEditMilliseconds = scenario.playbackMilliseconds - playbackBeforeEditMilliseconds;

        anasa::PhraseTileRenderer* neuralRenderer = nullptr;
        anasa::RendererFactory factory;

#ifdef ANASA_OPENVPI
        if (_neural)
        {
            factory = [this, &neuralRenderer](const anasa::AudioSettings& audio, const anasa::RenderSettings& render, const anasa::IChunkVersionReader& versions) -> std::unique_ptr<anasa::ITileRenderer>
            {
                const anasa::PhraseRenderFormat format = _timeline->format();

                if (audio.sampleRate != format.sampleRate || audio.channelCount != format.channelCount || render.layout.chunkFrames() != _timeline->phraseFrames() || render.layout.tilesPerChunk() != 1)
                    throw std::invalid_argument("Engine settings do not match the neural timeline");

                std::unique_ptr<anasa::IPhraseRenderer> backend = std::make_unique<anasa::OpenVpiRenderer>(_voicebank, _threads, _steps);
                std::unique_ptr<anasa::PhraseTileRenderer> renderer = std::make_unique<anasa::PhraseTileRenderer>(std::move(backend), _timeline, versions);

                neuralRenderer = renderer.get();
                return renderer;
            };
        }
#endif

        const Clock::time_point setupStart = Clock::now();
        anasa::Engine engine(engineSettings, factory);
        const double setupMilliseconds = std::chrono::duration<double, std::milli>(Clock::now() - setupStart).count();

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
            result.setupMilliseconds = setupMilliseconds;

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

            engine.rethrowRenderFailure();

            if (!engine.playbackSnapshot().playing)
                throw std::runtime_error("Playback stopped before the observation window ended");

            engine.stop();

            const anasa::EngineMetrics metrics = engine.metrics();

            result.callbacks = metrics.callbacks;
            result.underruns = metrics.underruns;
            result.maximumCallbackMicroseconds = metrics.maximumCallbackMicroseconds;

            engine.rethrowRenderFailure();

            if (neuralRenderer != nullptr)
                result.neural = neuralRenderer->metrics();

            const char* scenarioName = scenario.scenarioType == ScenarioType::SeekRecovery ? "seek" : scenario.scenarioType == ScenarioType::ViewportPreparedSeek ? "viewport" : "edit";
            const std::filesystem::path wavePath = _outputDirectory / (_neural ? "neural" : "synthetic") / (std::string(scenarioName) + "-" + policyName(policy) + "-" + std::to_string(trialNumber) + ".wav");

            const anasa::AudioBuffer recording = engine.capturedAudio();

            anasa::apps::writeStereoWave(wavePath, recording, engineSettings.audio.sampleRate);

            result.recordingMilliseconds = 1000.0 * recording.frameCount() / engineSettings.audio.sampleRate;
            result.wavePath = wavePath.string();

            return result;
        }
        catch (...)
        {
            engine.stop();
            throw;
        }
    }

    void addTrial(PolicyResult& result, const ScenarioSettings& scenario, int trialNumber)
    {
        std::cout << "Running " << scenario.name << " | "
                  << (_neural ? "OpenVPI" : "Synthetic") << " | "
                  << policyName(result.policy) << " | trial " << trialNumber << '/' << scenario.trialsPerPolicy
                  << " ... " << std::flush;

        const TrialResult trial = runTrial(result.policy, scenario, trialNumber);

        result.recoveryLatenciesMilliseconds.push_back(trial.recoveryMilliseconds);
        result.callbacks += trial.callbacks;
        result.underruns += trial.underruns;
        result.maximumCallbackMicroseconds = std::max(result.maximumCallbackMicroseconds, trial.maximumCallbackMicroseconds);

        std::cout << std::fixed << std::setprecision(2)
                  << trial.recoveryMilliseconds << " ms, "
                  << trial.underruns << " underruns, max callback=" << trial.maximumCallbackMicroseconds << " us\n"
                  << "  Setup=" << trial.setupMilliseconds << " ms (excluded from recovery)\n";

        if (_neural)
            std::cout << "  NN calls=" << trial.neural.inferenceCalls
                      << ", completed=" << trial.neural.completedPhrases
                      << ", cancelled=" << trial.neural.cancelledPhrases
                      << ", failed=" << trial.neural.failedPhrases
                      << ", render work=" << trial.neural.renderMilliseconds << " ms"
                      << '\n';

        std::cout << "  Recording=" << trial.recordingMilliseconds << " ms | WAV: " << trial.wavePath << '\n';
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
                  << " | Renderer: " << (_neural ? "OpenVPI CPU" : "Synthetic")
                  << " | Active playback: " << comparison.settings.playbackMilliseconds << " ms\n";

        if (comparison.settings.scenarioType == ScenarioType::ViewportPreparedSeek)
            std::cout << "Viewport preparation: " << comparison.settings.viewportPreparationMilliseconds << " ms\n";

        std::cout << '\n'
                  << std::left << std::setw(12) << "Policy"
                  << std::right << std::setw(18) << "Median recovery"
                  << std::setw(17) << "Worst recovery"
                  << std::setw(12) << "Underruns"
                  << std::setw(12) << "Callbacks"
                  << std::setw(16) << "Underrun rate"
                  << std::setw(19) << "Max callback (us)" << '\n';

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
                      << std::setw(15) << underrunPercentage(*result) << "%"
                      << std::setw(19) << result->maximumCallbackMicroseconds << '\n';
        }
    }

    static void printUsage()
    {
        std::cout << "Choose a runtime demo mode:\n"
                  << "  main synthetic [trials] [workIterations] [output-directory]\n"
                  << "  main neural <voicebank> <threads> <steps> <trials> <output-directory>\n"
                  << "  main both <voicebank> <threads> <steps> <trials> <output-directory> [workIterations]\n\n"
                  << "Synthetic defaults: 5 trials, 1000 work iterations, build/runtime-recordings\n"
                  << "Both runs synthetic first, then neural, with separate results and WAV folders.\n"
                  << "Each suite compares FIFO and Priority for seek, viewport and edit recovery.\n";
    }

    void configure(int argc, char** argv)
    {
        const std::string_view mode(argv[1]);

        if (mode == "synthetic")
        {
            if (argc > 5)
                throw std::invalid_argument("Usage: main synthetic [trials] [workIterations] [output-directory]");

            _mode = Mode::Synthetic;

            if (argc >= 3) _trialsPerPolicy = parseInteger(argv[2], 1, 100);
            if (argc >= 4) _workIterations = parseInteger(argv[3], 0, 100000);
            if (argc >= 5) _outputDirectory = argv[4];
        }
        else if (mode == "neural" || mode == "both")
        {
            if (argc != 7 && !(mode == "both" && argc == 8))
                throw std::invalid_argument("Usage: main neural|both <voicebank> <threads> <steps> <trials> <output-directory> [workIterations: both only]");

#ifndef ANASA_OPENVPI
            throw std::runtime_error("This executable has no neural backend. Configure with OPENVPI=ON and rebuild.");
#else
            _mode = mode == "both" ? Mode::Both : Mode::Neural;
            _voicebank = argv[2];
            _threads = parseInteger(argv[3], 1, 64);
            _steps = parseInteger(argv[4], 1, 100);
            _trialsPerPolicy = parseInteger(argv[5], 1, 100);
            _outputDirectory = argv[6];

            if (argc == 8) _workIterations = parseInteger(argv[7], 0, 100000);

            for (const char* file : {"dsacoustic/acoustic.onnx", "dsacoustic/phonemes.txt", "dsacoustic/tiger_fresh.emb", "dsvocoder/tgm_hifigan.onnx"})
                if (!std::filesystem::is_regular_file(_voicebank / file))
                    throw std::runtime_error("Missing voicebank file: " + (_voicebank / file).string());

            _timeline = makeTimeline();
#endif
        }
        else
            throw std::invalid_argument("Unknown mode. Use synthetic, neural, both, or --help.");

        if (_outputDirectory.empty())
            throw std::invalid_argument("Output directory must not be empty");

        if (_mode != Mode::Neural)
            std::filesystem::create_directories(_outputDirectory / "synthetic");

        if (_mode != Mode::Synthetic)
            std::filesystem::create_directories(_outputDirectory / "neural");
    }

    void runSuite(bool neural)
    {
        _neural = neural;

        std::cout << "\nRenderer: " << (_neural ? "OpenVPI CPU" : "Synthetic") << '\n';

        if (_neural)
        {
#ifdef ANASA_OPENVPI
            std::cout << "ONNX Runtime " << Ort::GetVersionString() << " | threads=" << _threads << " | steps=" << _steps << '\n';
#endif

            std::cout << "44100 Hz | identical L/R | one executor worker | one 88064-frame phrase per task\n"
                      << "Timeline: " << _timeline->totalFrames() << " frames (" << std::fixed << std::setprecision(3)
                      << 1000.0 * _timeline->totalFrames() / _timeline->format().sampleRate << " ms)\n"
                      << "Fresh model sessions and cache per trial; edit transposes the affected phrase by +3 semitones.\n";
        }
        else
            std::cout << "48000 Hz | 2 channels | " << anasa::ExecutorSettings{}.workerCount << " executor workers | 256-frame tiles\n"
                      << "Timeline: 12000 ms | work iterations=" << _workIterations << " | fresh cache per trial\n";

        std::cout << "Recovery ends when the new generation is acknowledged and playback is enabled.\n"
                  << "Underruns count active-playback callbacks; WAVs also include preparation silence.\n";

        const std::array<ScenarioSettings, 3> scenarios = makeScenarios();

        for (const ScenarioSettings& scenario : scenarios)
            printResults(runComparison(scenario));
    }

    static int parseInteger(std::string_view text, int minimum, int maximum)
    {
        int value = 0;
        const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), value);

        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || value < minimum || value > maximum)
            throw std::invalid_argument("Invalid numeric argument: " + std::string(text));

        return value;
    }

    static anasa::SingingRequest makeRequest(int transpose)
    {
        anasa::SingingRequest request;
        request.sampleRate = 44100;
        request.hopSize = 512;
        request.phonemes = {"SP", "l", "aa", "l", "aa", "l", "aa", "SP"};
        request.durations = {14, 5, 43, 5, 43, 5, 43, 14};
        request.f0.resize(172, 0.0f);

        const std::array<int, 3> notes{57, 60, 64};

        for (std::size_t note = 0; note < notes.size(); ++note)
        {
            const float frequency = static_cast<float>(440.0 * std::exp2((notes[note] + transpose - 69) / 12.0));
            std::fill_n(request.f0.data() + 14 + note * 48, 48, frequency);
        }

        return request;
    }

    static std::shared_ptr<const anasa::PreparedSingingTimeline> makeTimeline()
    {
        const std::array<int, 6> transpositions{0, 2, -2, 0, 2, -2};

        std::vector<std::vector<anasa::SingingRequest>> phrases;
        phrases.reserve(transpositions.size());

        for (const int transpose : transpositions)
            phrases.push_back({makeRequest(transpose), makeRequest(transpose + 3)});

        return std::make_shared<const anasa::PreparedSingingTimeline>(anasa::PhraseRenderFormat{44100, 2, 512}, std::move(phrases));
    }

    Mode _mode = Mode::Synthetic;
    bool _neural = false;
    int _workIterations = 1000;
    int _threads = 4;
    int _steps = 20;
    int _trialsPerPolicy = 5;

    std::filesystem::path _voicebank;
    std::filesystem::path _outputDirectory = "build/runtime-recordings";
    std::shared_ptr<const anasa::PreparedSingingTimeline> _timeline;
};

int main(int argc, char** argv)
{
    try
    {
        RuntimeDemo demo;
        return demo.run(argc, argv);
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Comparison failed: " << exception.what() << '\n';
        return 1;
    }
}