#include "render/openvpi/OpenVpiRenderer.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace
{
    using Clock = std::chrono::steady_clock;

    double milliseconds(Clock::time_point start, Clock::time_point end)
    {
        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    std::int64_t checkedSteps(int steps)
    {
        if (steps < 1 || steps > 100)
            throw std::invalid_argument("steps must be in [1, 100]");
        return steps;
    }

    Ort::Env makeEnvironment()
    {
        Ort::Env environment(ORT_LOGGING_LEVEL_WARNING, "anasa-openvpi");
        environment.DisableTelemetryEvents();
        return environment;
    }

    Ort::SessionOptions makeOptions(int threads)
    {
        if (threads < 1 || threads > 64)
            throw std::invalid_argument("threads must be in [1, 64]");
        Ort::SessionOptions options;
        options.SetIntraOpNumThreads(threads);
        options.SetInterOpNumThreads(1);
        options.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        options.AddConfigEntry("session.intra_op.allow_spinning", "0");
        return options;
    }

    std::array<float, 256> readSpeaker(const std::filesystem::path& path)
    {
        static_assert(std::endian::native == std::endian::little && sizeof(float) == 4);
        std::array<float, 256> speaker{};
        std::ifstream file(path, std::ios::binary);
        if (!file || std::filesystem::file_size(path) != sizeof(speaker))
            throw std::runtime_error("Expected a 256-float speaker embedding: " + path.string());
        if (!file.read(reinterpret_cast<char*>(speaker.data()), sizeof(speaker)))
            throw std::runtime_error("Could not read speaker embedding");
        for (float value : speaker)
            if (!std::isfinite(value))
                throw std::runtime_error("Non-finite speaker embedding");
        return speaker;
    }

    std::unordered_map<std::string, std::int64_t> readPhonemes(const std::filesystem::path& path)
    {
        std::ifstream file(path);
        if (!file)
            throw std::runtime_error("Could not open phoneme table: " + path.string());
        std::unordered_map<std::string, std::int64_t> ids;
        std::string phone;
        std::int64_t index = 0;
        while (file >> phone)
            if (!ids.emplace(phone, index++).second)
                throw std::runtime_error("Duplicate phoneme: " + phone);
        if (file.bad() || ids.empty())
            throw std::runtime_error("Invalid phoneme table");
        return ids;
    }

    template<class T, std::size_t Rank>
    Ort::Value tensor(const Ort::MemoryInfo& memory, T* data, std::size_t count,
                      const std::array<std::int64_t, Rank>& shape)
    {
        return Ort::Value::CreateTensor<T>(memory, data, count, shape.data(), shape.size());
    }

    void checkShape(const Ort::Value& value, const std::vector<std::int64_t>& expected)
    {
        if (!value.IsTensor())
            throw std::runtime_error("Model output is not a tensor");
        const auto info = value.GetTensorTypeAndShapeInfo();
        if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT || info.GetShape() != expected)
            throw std::runtime_error("Unexpected model output type or shape");
    }
}

namespace anasa
{
    OpenVpiRenderer::OpenVpiRenderer(const std::filesystem::path& voicebank, int threads, int steps)
        : _steps(checkedSteps(steps)),
          _environment(makeEnvironment()),
          _options(makeOptions(threads)),
          _acoustic(_environment, (voicebank / "dsacoustic/acoustic.onnx").c_str(), _options),
          _vocoder(_environment, (voicebank / "dsvocoder/tgm_hifigan.onnx").c_str(), _options),
          _speaker(readSpeaker(voicebank / "dsacoustic/tiger_fresh.emb")),
          _phonemes(readPhonemes(voicebank / "dsacoustic/phonemes.txt"))
    {}

    OpenVpiRenderResult OpenVpiRenderer::render(const SingingRequest& request)
    {
        if (request.phonemes.empty() || request.phonemes.size() != request.durations.size())
            throw std::invalid_argument("Provide one duration per phoneme");

        if (request.f0.empty() || request.f0.size() > MAX_MEL_FRAMES)
            throw std::invalid_argument("Phrase must contain 1..4096 mel frames");

        const auto frames = static_cast<std::int64_t>(request.f0.size());
        
        std::int64_t total = 0;
        for (std::int64_t duration : request.durations)
        {
            if (duration <= 0 || duration > frames - total)
                throw std::invalid_argument("Invalid phoneme duration");

            total += duration;
        }
        
        if (total != frames)
            throw std::invalid_argument("Durations must sum to the f0 frame count");
        
        for (float frequency : request.f0)
            if (!std::isfinite(frequency) || frequency < 0.0f)
                throw std::invalid_argument("f0 must contain finite non-negative frequencies");

        std::vector<std::int64_t> tokens;
        tokens.reserve(request.phonemes.size());
        
        for (const auto& phone : request.phonemes)
        {
            const auto found = _phonemes.find(phone);
            if (found == _phonemes.end())
                throw std::invalid_argument("Unknown phoneme: " + phone);
            tokens.push_back(found->second);
        }

        // Own writable backing storage for ORT tensor views; keep it alive through both Run calls.
        auto durations = request.durations;
        auto f0 = request.f0;
        std::vector<float> gender(f0.size(), 0.0f);
        std::vector<float> velocity(f0.size(), 1.0f);
        std::vector<float> speaker(f0.size() * _speaker.size());
        
        for (std::size_t frame = 0; frame < f0.size(); ++frame)
            std::copy(_speaker.begin(), _speaker.end(), speaker.data() + frame * _speaker.size());
        
        float depth = 0.6f;
        std::int64_t steps = _steps;

        const std::array<std::int64_t, 2> phoneShape{1, static_cast<std::int64_t>(tokens.size())};
        const std::array<std::int64_t, 2> frameShape{1, frames};
        const std::array<std::int64_t, 3> speakerShape{1, frames, 256};
        const std::array<std::int64_t, 0> scalarShape{};
        auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        
        std::array<Ort::Value, 8> inputs{
            tensor(memory, tokens.data(), tokens.size(), phoneShape),
            tensor(memory, durations.data(), durations.size(), phoneShape),
            tensor(memory, f0.data(), f0.size(), frameShape),
            tensor(memory, gender.data(), gender.size(), frameShape),
            tensor(memory, velocity.data(), velocity.size(), frameShape),
            tensor(memory, speaker.data(), speaker.size(), speakerShape),
            tensor(memory, &depth, 1, scalarShape),
            tensor(memory, &steps, 1, scalarShape)
        };
        
        const std::array<const char*, 8> names{
            "tokens", "durations", "f0", "gender", "velocity", "spk_embed", "depth", "steps"
        };
        const char* melName = "mel";
        Ort::RunOptions runOptions;
        const auto acousticStart = Clock::now();
        auto mel = _acoustic.Run(runOptions, names.data(), inputs.data(), inputs.size(), &melName, 1);
        const auto acousticEnd = Clock::now();
        checkShape(mel.at(0), {1, frames, 128});

        std::array<Ort::Value, 2> vocoderInputs{
            std::move(mel[0]), tensor(memory, f0.data(), f0.size(), frameShape)
        };
        
        const std::array<const char*, 2> vocoderNames{"mel", "f0"};
        const char* waveformName = "waveform";
        const auto vocoderStart = Clock::now();
        auto waveform = _vocoder.Run(runOptions, vocoderNames.data(), vocoderInputs.data(),
                                     vocoderInputs.size(), &waveformName, 1);
        const auto vocoderEnd = Clock::now();
        checkShape(waveform.at(0), {1, frames * HOP_SIZE});

        OpenVpiRenderResult result{AudioBuffer(CHANNELS, static_cast<int>(frames * HOP_SIZE))};
        const float* mono = waveform[0].GetTensorData<float>();
        
        for (int frame = 0; frame < result.samples.frameCount(); ++frame)
        {
            if (!std::isfinite(mono[frame]))
                throw std::runtime_error("Non-finite model output");
            result.samples[0][frame] = mono[frame];
            result.samples[1][frame] = mono[frame];
        }
        
        result.acousticMs = milliseconds(acousticStart, acousticEnd);
        result.vocoderMs = milliseconds(vocoderStart, vocoderEnd);
        return result;
    }
}