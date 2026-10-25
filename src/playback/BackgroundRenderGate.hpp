#ifndef BACKGROUND_RENDER_GATE_HPP
#define BACKGROUND_RENDER_GATE_HPP

namespace anasa
{
    // Playback-headroom policy.
    class BackgroundRenderGate
    {
    public:
        BackgroundRenderGate(int lowWaterBlocks, int highWaterBlocks);

        void observe(int readyLeadBlocks) noexcept;
        void reset() noexcept;
        bool allowsBackground() const noexcept;

    private:
        const int _lowWaterBlocks;
        const int _highWaterBlocks;
        bool      _allowed = true;
    };
} // namespace anasa

#endif // BACKGROUND_RENDER_GATE_HPP