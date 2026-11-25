// gba_raster_workers.h — optional scanline rasterization on worker threads.
//
// The guest thread only records what each visible line looked like at its
// HBlank (registers, OAM, palette) plus one VRAM snapshot per frame. Two
// worker threads turn a complete recording into pixels, each taking half the
// screen. When the workers are still busy at the start of a frame, that frame
// is not recorded at all, so slow rendering lowers the displayed rate and
// never the emulated speed.
//
// Presentation only: no guest state or device time depends on it. A VRAM write
// made during the visible period appears one frame late.
#pragma once

#include <cstddef>
#include <cstdint>

namespace gba {

class RasterWorkers {
public:
    using LineFn = void (*)(std::uint8_t* rgb, std::uint32_t y, std::uint16_t dispcnt,
                            const std::uint8_t* io, const std::uint8_t* vram,
                            const std::uint8_t* oam, const std::uint8_t* pal);

    // Start the threads. Returns false (and stays disabled) if the platform
    // has no implementation or thread creation fails.
    static bool start(LineFn render_line);
    static bool enabled();

    // Guest thread: record one line. `io` is 0x60 bytes.
    static void record_line(std::uint32_t y, std::uint16_t dispcnt,
                            const std::uint8_t* io, const std::uint8_t* vram,
                            const std::uint8_t* oam, const std::uint8_t* pal);
    // Guest thread, at VBlank: queue the recording, then copy the newest
    // finished picture to `out` (240*160*3). Returns true if it was new.
    static bool frame_boundary(std::uint8_t* out);

    static std::uint32_t frames_rendered();
    static std::uint32_t frames_dropped();
};

} // namespace gba
