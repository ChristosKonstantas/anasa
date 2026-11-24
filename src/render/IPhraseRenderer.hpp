#ifndef I_PHRASE_RENDERER_HPP
#define I_PHRASE_RENDERER_HPP

#include "render/PhraseRenderTypes.hpp"
#include <stop_token>

namespace anasa
{
    // Whole-phrase rendering. Calls may allocate and block therefore never use this on the audio thread.
    // One caller at a time per instance. Requests remain unchanged until render() returns.
    class IPhraseRenderer
    {
    public:
        virtual ~IPhraseRenderer() = default;

        IPhraseRenderer(const IPhraseRenderer&) = delete;
        IPhraseRenderer& operator=(const IPhraseRenderer&) = delete;
        IPhraseRenderer(IPhraseRenderer&&) = delete;
        IPhraseRenderer& operator=(IPhraseRenderer&&) = delete;

        virtual PhraseRenderFormat format() const noexcept = 0;

        // Success returns owned PCM matching format(), with f0.size() * hopSize frames.
        // Cancellation throws PhraseRenderCancelled. Other failures retain their error.
        // Cancellation is cooperative, with no guaranteed interruption time.
        // Request stop from a control/worker thread. Stop callbacks may run synchronously.
        virtual PhraseRenderResult render(const SingingRequest& request, std::stop_token stopToken = {}) = 0;

    protected:
        IPhraseRenderer() = default;
    };
}

#endif // I_PHRASE_RENDERER_HPP