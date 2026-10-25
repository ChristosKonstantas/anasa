#include "scheduler/RenderCache.hpp"

#include <span>
#include <stdexcept>

namespace anasa
{
    RenderCache::RenderCache(int channelCount, int totalFrames, const IChunkVersionReader& versions)
        : _channelCount(channelCount),
          _versions(versions)
    {
        if (_channelCount <= 0)
            throw std::invalid_argument("channelCount must be greater than zero");

        if (totalFrames <= 0)
            throw std::invalid_argument("totalFrames must be greater than zero");

        const int expectedChunkCount = 1 + (totalFrames - 1) / CHUNK_FRAMES;

        if (_versions.count() != expectedChunkCount)
            throw std::invalid_argument("VersionTable size does not match timeline chunk count");

        _cacheEntries.reserve(static_cast<std::size_t>(expectedChunkCount));

        for (int chunk = 0; chunk < expectedChunkCount; ++chunk)
            _cacheEntries.emplace_back(_channelCount);
    }

    int RenderCache::chunkCount() const noexcept
    {
        return static_cast<int>(_cacheEntries.size());
    }

    const AudioBuffer* RenderCache::findCurrent(int chunk) const
    {
        if (chunk < 0 || chunk >= chunkCount())
            return nullptr;

        const CacheEntry& cacheEntry = _cacheEntries[chunk];

        if (cacheEntry.version != _versions.get(chunk))
            return nullptr;

        return &cacheEntry.samples;
    }

    bool RenderCache::store(int chunk, int version, const AudioBuffer& samples)
    {
        if (chunk < 0 || chunk >= chunkCount())
            return false;

        if (version != _versions.get(chunk) || samples.channelCount() != _channelCount || samples.frameCount() != CHUNK_FRAMES)
            return false;

        CacheEntry& cacheEntry = _cacheEntries[chunk];

        for (int channel = 0; channel < _channelCount; ++channel)
        {
            const std::span<const float> source = samples[channel];
            const std::span<float> destination = cacheEntry.samples[channel];

            for (int frame = 0; frame < CHUNK_FRAMES; ++frame)
                destination[frame] = source[frame];
        }

        cacheEntry.version = version;
        return true;
    }
} // namespace anasa