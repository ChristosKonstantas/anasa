#include "render/PhraseTileRenderer.hpp"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace anasa
{
    PhraseTileRenderer::PhraseTileRenderer(std::unique_ptr<IPhraseRenderer> renderer,  std::shared_ptr<const PreparedSingingTimeline> timeline, const IChunkVersionReader& versions)
        : _renderer(std::move(renderer)),
          _timeline(std::move(timeline)),
          _versions(versions),
          _cancellation(versions)
    {
        if (!_renderer || !_timeline)
            throw std::invalid_argument("Phrase renderer and timeline must not be null");

        const PhraseRenderFormat format   = _renderer->format();
        const PhraseRenderFormat expected = _timeline->format();

        if (format.sampleRate != expected.sampleRate || format.channelCount != expected.channelCount || format.hopSize != expected.hopSize || _versions.count() != _timeline->phraseCount())
            throw std::invalid_argument("Phrase renderer, timeline and version table do not match");
    }

    bool PhraseTileRenderer::renderTile(RenderJob& job, int tileIndex, const std::atomic<bool>& stopRequested) const
    {
        std::lock_guard<std::mutex> lock(_mutex);

        const PhraseRenderFormat format = _timeline->format();

        if (tileIndex != 0 ||
            job.layout.tilesPerChunk() != 1 ||
            job.layout.chunkFrames() != _timeline->phraseFrames() ||
            job.samples.frameCount() != _timeline->phraseFrames() ||
            job.samples.channelCount() != format.channelCount ||
            job.chunk < 0 ||
            job.chunk >= _versions.count())
        {
            throw std::invalid_argument("Phrase rendering requires one correctly sized tile per job");
        }

        if (_cancellation.shouldCancel(job, stopRequested))
        {
            job.cancel();
            return false;
        }

        const SingingRequest& request = _timeline->request(job.chunk, job.version);

        ++_metrics.inferenceCalls;
        const Clock::time_point start = Clock::now();

        try
        {
            PhraseRenderResult result = _renderer->render(request, job.cancellationToken());

            if (_cancellation.shouldCancel(job, stopRequested))
            {
                job.cancel();
                ++_metrics.cancelledPhrases;
                recordElapsed(start);
                return false;
            }

            if (result.sampleRate != format.sampleRate || result.samples.channelCount() != format.channelCount || result.samples.frameCount() != job.samples.frameCount())
                throw std::runtime_error("Phrase renderer returned an unexpected audio format");

            for (int channel = 0; channel < job.samples.channelCount(); ++channel)
            {
                for (int frame = 0; frame < job.samples.frameCount(); ++frame)
                {
                    const float sample = result.samples[channel][frame];

                    if (!std::isfinite(sample))
                        throw std::runtime_error(
                            "Phrase renderer returned non-finite audio");

                    job.samples[channel][frame] = sample;
                }
            }

            if (_cancellation.shouldCancel(job, stopRequested))
            {
                job.cancel();
                ++_metrics.cancelledPhrases;
                recordElapsed(start);
                return false;
            }

            ++_metrics.completedPhrases;
            recordElapsed(start);
            return true;
        }
        catch (const PhraseRenderCancelled&)
        {
            job.cancel();
            ++_metrics.cancelledPhrases;
            recordElapsed(start);
            return false;
        }
        catch (...)
        {
            ++_metrics.failedPhrases;
            recordElapsed(start);
            throw;
        }
    }

    void PhraseTileRenderer::recordElapsed(Clock::time_point start) const
    {
        _metrics.renderMilliseconds += std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }

    PhraseRenderMetrics PhraseTileRenderer::metrics() const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _metrics;
    }
}