#ifndef RENDER_CONSTANTS_HPP
#define RENDER_CONSTANTS_HPP

namespace anasa
{

// At 48kHz, one tile is 256 frames ~5.33 ms of audio
// One chunk is 2048 frames ~42.67 ms of audio
// One chunk contains 8 independently scheduled tiles

constexpr int CHUNK_FRAMES = 2048;
constexpr int TILE_FRAMES = 256;

constexpr int TILES_PER_CHUNK = CHUNK_FRAMES / TILE_FRAMES;

constexpr int CANCELLATION_CHECK_FRAMES = 32;

static_assert(CHUNK_FRAMES % TILE_FRAMES == 0, "CHUNK_FRAMES must be divisible by TILE_FRAMES");

} // namespace anasa

#endif // RENDER_CONSTANTS_HPP