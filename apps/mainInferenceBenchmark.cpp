#include "render/openvpi/OpenVpiRenderer.hpp"
#include "render/kernels/PcmRenderKernel.hpp"
#include "render/Renderer.hpp"
#include "render/VersionTable.hpp"
#include "WaveWriter.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    using Clock = std::chrono::steady_clock;

    double elapsedMs(Clock::time_point start)
    {
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }

    int argument(std::string_view text, int minimum, int maximum)
    {
        int value = 0;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error != std::errc{} || end != text.data() + text.size() || value < minimum || value > maximum)
            throw std::invalid_argument("Invalid numeric argument: " + std::string(text));
        return value;
    }

    anasa::SingingRequest makeExample(int transpose)
    {
        anasa::SingingRequest request;
        request.phonemes = {"SP", "l", "aa", "l", "aa", "l", "aa", "SP"};
        request.durations = {14, 5, 43, 5, 43, 5, 43, 14};
        request.f0.resize(172, 0.0f);
        const std::array<int, 3> notes{57, 60, 64};
        for (std::size_t note = 0; note < notes.size(); ++note)
        {
            const float hz = static_cast<float>(440.0 * std::exp2((notes[note] + transpose - 69) / 12.0));
            std::fill_n(request.f0.data() + 14 + note * 48, 48, hz);
        }
        return request;
    }

    struct AudioStatistics
    {
        double checksum = 0.0; // Sum of squared samples across both channels.
        double rms = 0.0;
        double peak = 0.0;
        std::size_t clippedSamples = 0;
    };

    AudioStatistics checkAudio(const anasa::AudioBuffer& samples)
    {
        if (samples.channelCount() != 2 || samples.frameCount() != 172 * anasa::OpenVpiRenderer::HOP_SIZE)
            throw std::runtime_error("Unexpected fixture audio dimensions");
        AudioStatistics result;
        for (int frame = 0; frame < samples.frameCount(); ++frame)
        {
            if (samples[0][frame] != samples[1][frame])
                throw std::runtime_error("Expected identical left/right channels");
            for (int channel = 0; channel < samples.channelCount(); ++channel)
            {
                const double value = samples[channel][frame];
                if (!std::isfinite(value))
                    throw std::runtime_error("Non-finite audio");
                result.checksum += value * value;
                result.peak = std::max(result.peak, std::abs(value));
                result.clippedSamples += std::abs(value) > 1.0;
            }
        }
        if (!std::isfinite(result.checksum) || result.checksum <= 0.0)
            throw std::runtime_error("Invalid or silent output");
        result.rms = std::sqrt(result.checksum / (2.0 * samples.frameCount()));
        return result;
    }

    // Exercise the existing interfaces using completed PCM. No inference occurs here.
    anasa::AudioBuffer deliverTiles(const anasa::AudioBuffer& source)
    {
        using namespace anasa;

        const int chunks = 1 + (source.frameCount() - 1) / CHUNK_FRAMES;
        VersionTable versions(chunks);
        std::unique_ptr<IRenderKernel> kernel = std::make_unique<PcmRenderKernel>(source, 1);
        Renderer renderer(std::move(kernel), versions);
        const ITileRenderer& tileRenderer = renderer;
        RenderJob job(source.channelCount());
        std::atomic<bool> stopRequested{false};
        AudioBuffer delivered(source.channelCount(), source.frameCount());

        const auto start = Clock::now();
        for (int chunk = 0; chunk < chunks; ++chunk)
        {
            job.chunk = chunk;
            job.version = versions.get(chunk);
            for (int tile = 0; tile < TILES_PER_CHUNK; ++tile)
                if (!tileRenderer.renderTile(job, tile, stopRequested))
                    throw std::runtime_error("PCM tile delivery cancelled");
            const int first = chunk * CHUNK_FRAMES;
            const int count = std::min(CHUNK_FRAMES, source.frameCount() - first);
            for (int channel = 0; channel < source.channelCount(); ++channel)
                std::copy_n(job.samples[channel].data(), count, delivered[channel].data() + first);
        }
        const double deliveryMs = elapsedMs(start);

        for (int channel = 0; channel < source.channelCount(); ++channel)
            if (!std::equal(source[channel].begin(), source[channel].end(), delivered[channel].begin()))
                throw std::runtime_error("Tile delivery changed the PCM");
                
        std::cout << "PCM tile delivery: " << deliveryMs << " ms (not inference)\n";
        return delivered;
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc != 6 && argc != 7)
            throw std::invalid_argument(
                "Usage: anasa-inference-benchmark <voicebank> <threads> <steps> <trials> <output.wav> [transpose]");
        const int threads = argument(argv[2], 1, 64);
        const int steps = argument(argv[3], 1, 100);
        const int trials = argument(argv[4], 1, 100);
        const int transpose = argc == 7 ? argument(argv[6], -12, 12) : 0;
        const auto request = makeExample(transpose);
        std::cout << std::fixed << std::setprecision(3)
                  << "ONNX Runtime: " << Ort::GetVersionString() << '\n'
                  << "Provider: CPU | Intra-op threads: " << threads << " | Serial requests\n"
                  << "TIGER v106/fresh | Steps: " << steps << " | Depth: 0.6\n"
                  << "Prepared phrase: la la la | MIDI: " << 57 + transpose << ',' << 60 + transpose << ',' << 64 + transpose << '\n'
                  << "Channels: 2 (identical L/R) | Sample rate: 44100 Hz\n"
                  << "Mel frames: 172 | Audio frames: 88064 | Measured trials: " << trials << '\n';

        const auto setupStart = Clock::now();
        anasa::OpenVpiRenderer renderer(std::filesystem::path(argv[1]), threads, steps);
        std::cout << "Model setup: " << elapsedMs(setupStart) << " ms\n" << std::flush;

        const auto firstStart = Clock::now();
        auto first = renderer.render(request);
        const double firstMs = elapsedMs(firstStart);
        checkAudio(first.samples);
        std::cout << "First render: " << firstMs << " ms (excluded from warm trials)\n" << std::flush;
        checkAudio(renderer.render(request).samples); // One additional warm-up.

        std::vector<double> times;
        times.reserve(static_cast<std::size_t>(trials));
        anasa::AudioBuffer lastAudio;
        double checksum = 0.0;
        const double audioMs = 1000.0 * 172 * anasa::OpenVpiRenderer::HOP_SIZE / anasa::OpenVpiRenderer::SAMPLE_RATE;
        std::cout << "trial,acoustic_ms,vocoder_ms,total_ms,audio_ms,rtf,rms,peak,clipped_samples\n" << std::flush;
        for (int trial = 1; trial <= trials; ++trial)
        {
            const auto start = Clock::now();
            auto result = renderer.render(request);
            const double totalMs = elapsedMs(start); // Includes temporary cleanup before render() returns.
            if (!std::isfinite(totalMs) || totalMs <= 0.0)
                throw std::runtime_error("Invalid timing measurement");
            const auto statistics = checkAudio(result.samples); // Outside inference timing.
            times.push_back(totalMs);
            checksum += statistics.checksum;
            std::cout << trial << ',' << result.acousticMs << ',' << result.vocoderMs << ','
                      << totalMs << ',' << audioMs << ',' << totalMs / audioMs << ','
                      << statistics.rms << ',' << statistics.peak << ',' << statistics.clippedSamples << '\n' << std::flush;
            lastAudio = std::move(result.samples);
        }

        std::sort(times.begin(), times.end());
        const double mean = std::accumulate(times.begin(), times.end(), 0.0) / static_cast<double>(times.size());
        const std::size_t middle = times.size() / 2;
        const double median = times.size() % 2 ? times[middle] : (times[middle - 1] + times[middle]) / 2.0;
        std::cout << "Mean phrase time: " << mean << " ms\n"
                  << "Median phrase time: " << median << " ms\n"
                  << "Worst observed phrase time: " << times.back() << " ms\n"
                  << "Operations per second (phrases): " << 1000.0 / mean << '\n'
                  << "Mean serial render RTF: " << mean / audioMs << '\n'
                  << "Checksum (both channels, measured trials): " << checksum << '\n';

        auto delivered = deliverTiles(lastAudio);
        anasa::apps::writeStereoWave(std::filesystem::path(argv[5]), delivered, anasa::OpenVpiRenderer::SAMPLE_RATE);
        std::cout << "WAV saved: " << argv[5] << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}