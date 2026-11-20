#ifndef PREPARED_SINGING_TIMELINE_HPP
#define PREPARED_SINGING_TIMELINE_HPP

#include "render/PhraseRenderTypes.hpp"
#include <limits>
#include <utility>

namespace anasa
{
    // Immutable prepared content with numbered revisions.
    // This initial timeline uses equal-length phrases.
    class PreparedSingingTimeline
    {
    public:
        PreparedSingingTimeline(PhraseRenderFormat format, std::vector<std::vector<SingingRequest>> phrases)
            : _format(format), 
              _phrases(std::move(phrases))
        {
            if (_format.sampleRate <= 0 ||
                _format.channelCount <= 0 ||
                _format.hopSize <= 0 ||
                _phrases.empty() ||
                _phrases.front().empty())
            {
                throw std::invalid_argument("Invalid prepared singing timeline");
            }

            const std::size_t melFrames = _phrases.front().front().f0.size();

            if (melFrames == 0 ||  melFrames > static_cast<std::size_t>(std::numeric_limits<int>::max() / _format.hopSize))
                throw std::invalid_argument("Invalid phrase frame count");

            _phraseFrames = static_cast<int>(melFrames) * _format.hopSize;

            if (_phrases.size() > static_cast<std::size_t>(std::numeric_limits<int>::max() / _phraseFrames))
                throw std::length_error("Singing timeline exceeds the supported frame range");

            for (const std::vector<SingingRequest>& revisions : _phrases)
            {
                if (revisions.empty())
                    throw std::invalid_argument("Every phrase needs an initial revision");

                for (const SingingRequest& request : revisions)
                {
                    if (request.sampleRate != _format.sampleRate || request.hopSize != _format.hopSize || request.f0.size() != melFrames)
                        throw std::invalid_argument("Phrase revisions must use the timeline format and length");
                }
            }
        }

        PhraseRenderFormat format() const noexcept { return _format; }
        int phraseFrames() const noexcept { return _phraseFrames; }
        int phraseCount() const noexcept { return static_cast<int>(_phrases.size()); }
        int totalFrames() const noexcept { return phraseCount() * _phraseFrames; }

        const SingingRequest& request(int phrase, int version) const
        {
            if (phrase < 0 || phrase >= phraseCount() || version <= 0 || static_cast<std::size_t>(version) > _phrases[phrase].size())
                throw std::out_of_range("No prepared singing content for this phrase revision");

            return _phrases[phrase][static_cast<std::size_t>(version - 1)];
        }

    private:
        PhraseRenderFormat                       _format;
        std::vector<std::vector<SingingRequest>> _phrases;
        int                                      _phraseFrames = 0;
    };
}

#endif // PREPARED_SINGING_TIMELINE_HPP