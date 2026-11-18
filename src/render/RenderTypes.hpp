#ifndef RENDER_TYPES_HPP
#define RENDER_TYPES_HPP

#include <atomic>
#include <stop_token>

#include "audio/AudioBuffer.hpp"
#include "render/RenderLayout.hpp"

namespace anasa
{
    struct RenderJob
    {
    public:
        explicit RenderJob(int channelCount,
                           RenderLayout renderLayout = RenderLayout())
            : layout(renderLayout),
              tilesRemaining(layout.tilesPerChunk()),
              samples(channelCount, layout.chunkFrames())
        {}

        bool isCancelled() const noexcept
        {
            return _cancellation.stop_requested();
        }

        void cancel() noexcept
        {
            _cancellation.request_stop();
        }

        std::stop_token cancellationToken() const noexcept
        {
            return _cancellation.get_token();
        }

        const RenderLayout layout;
        int chunk = 0;
        int version = 0;
        std::atomic<int> tilesRemaining;

        // Dimensions remain fixed while the job is being rendered.
        AudioBuffer samples;

    private:
        std::stop_source _cancellation;
    };

    struct CacheEntry
    {
        explicit CacheEntry(int channelCount, int chunkFrames = CHUNK_FRAMES)
            : samples(channelCount, chunkFrames)
        {}

        int version = 0;
        AudioBuffer samples;
    };
}

#endif // RENDER_TYPES_HPP