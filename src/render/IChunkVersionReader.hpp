#ifndef I_CHUNK_VERSION_READER_HPP
#define I_CHUNK_VERSION_READER_HPP

namespace anasa
{
    // Read-only access to content versions; mutation remains with the owner.
    class IChunkVersionReader
    {
    public:
        virtual ~IChunkVersionReader() = default;

        IChunkVersionReader(const IChunkVersionReader&) = delete;
        IChunkVersionReader& operator=(const IChunkVersionReader&) = delete;
        IChunkVersionReader(IChunkVersionReader&&) = delete;
        IChunkVersionReader& operator=(IChunkVersionReader&&) = delete;

        virtual int get(int chunk) const = 0;
        virtual int count() const = 0;

    protected:
        IChunkVersionReader() = default;
    };
}

#endif // I_CHUNK_VERSION_READER_HPP