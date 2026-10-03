#ifndef I_RENDER_CACHE_READER_HPP
#define I_RENDER_CACHE_READER_HPP

#include "audio/AudioBuffer.hpp"

namespace anasa
{
    class IRenderCacheReader
    {
    public:
        virtual ~IRenderCacheReader() = default;

        IRenderCacheReader(const IRenderCacheReader&) = delete;
        IRenderCacheReader& operator=(const IRenderCacheReader&) = delete;
        IRenderCacheReader(IRenderCacheReader&&) = delete;
        IRenderCacheReader& operator=(IRenderCacheReader&&) = delete;

        virtual int chunkCount() const noexcept = 0;

        virtual const AudioBuffer* findCurrent(int chunk) const = 0;

    protected:
        IRenderCacheReader() = default;
    };
} // namespace anasa

#endif // I_RENDER_CACHE_READER_HPP