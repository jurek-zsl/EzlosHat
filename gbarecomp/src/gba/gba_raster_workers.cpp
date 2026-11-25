// gba_raster_workers.cpp — see gba_raster_workers.h.

#include "gba_raster_workers.h"

#include <array>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>

#if defined(__vita__)
#include <psp2/kernel/cpu.h>
#include <psp2/kernel/threadmgr.h>
#else
#include <condition_variable>
#include <mutex>
#include <thread>
#endif

namespace gba {
namespace {

constexpr unsigned kLines = 160;
constexpr unsigned kWidth = 240;
constexpr std::size_t kVramBytes = 96u * 1024u;
constexpr std::size_t kRgbBytes = kWidth * kLines * 3u;
constexpr unsigned kWorkers = 2;
constexpr unsigned kJobs = 3;       // recording + queued/rendering + spare
constexpr unsigned kMaxInFlight = 1; // bounds added display latency

struct Line {
    std::uint16_t dispcnt;
    std::uint8_t io[0x60];
    std::uint8_t oam[1024];
    std::uint8_t pal[1024];
};

struct Job {
    Line lines[kLines];
    std::uint8_t vram[kVramBytes];
    std::uint8_t rgb[kRgbBytes];
    std::atomic<unsigned> remaining{0}; // workers still rendering; 0 = idle/done
};

// A counting semaphore; std::thread and friends do not work on the Vita.
class Semaphore {
public:
#if defined(__vita__)
    bool init(const char* name) {
        id_ = sceKernelCreateSema(name, 0, 0, kJobs, nullptr);
        return id_ >= 0;
    }
    void post() { sceKernelSignalSema(id_, 1); }
    void wait() { sceKernelWaitSema(id_, 1, nullptr); }
private:
    SceUID id_ = -1;
#else
    bool init(const char*) { return true; }
    void post() {
        { std::lock_guard<std::mutex> lock(mutex_); ++count_; }
        cv_.notify_one();
    }
    void wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return count_ > 0; });
        --count_;
    }
private:
    std::mutex mutex_;
    std::condition_variable cv_;
    int count_ = 0;
#endif
};

// Intentionally never destroyed: workers may be blocked here at process exit.
struct State {
    std::array<Job, kJobs> jobs;
    std::array<Semaphore, kWorkers> wake;
    // Submission ring. Written by the guest thread before it posts, and read
    // by workers after they wait, so the semaphore orders the accesses.
    std::array<Job*, kJobs * 2> ring{};
    unsigned submitted = 0;   // guest thread only
    unsigned collected = 0;   // guest thread only
    Job* recording = nullptr; // guest thread only
    unsigned recorded = 0;
    std::uint32_t rendered = 0, dropped = 0;
    RasterWorkers::LineFn render_line = nullptr;
};
State* g_state = nullptr;

void worker_loop(unsigned index) {
    State& s = *g_state;
    const unsigned first = index * kLines / kWorkers;
    const unsigned last = (index + 1) * kLines / kWorkers;
    for (unsigned sequence = 0;; ++sequence) {
        s.wake[index].wait();
        Job& job = *s.ring[sequence % s.ring.size()];
        for (unsigned y = first; y < last; ++y) {
            const Line& line = job.lines[y];
            s.render_line(job.rgb, y, line.dispcnt, line.io, job.vram, line.oam, line.pal);
        }
        job.remaining.fetch_sub(1, std::memory_order_release);
    }
}

#if defined(__vita__)
int worker_entry(SceSize, void* argument) {
    worker_loop(*static_cast<unsigned*>(argument));
    return 0;
}
#endif

Job* free_job(State& s) {
    const unsigned in_flight = s.submitted - s.collected;
    if (in_flight > kMaxInFlight) return nullptr;
    for (auto& job : s.jobs) {
        bool used = false;
        for (unsigned n = s.collected; n != s.submitted; ++n)
            used |= s.ring[n % s.ring.size()] == &job;
        if (!used) return &job;
    }
    return nullptr;
}

} // namespace

bool RasterWorkers::start(LineFn render_line) {
    if (g_state) return true;
    auto* state = new (std::nothrow) State;
    if (!state) return false;
    state->render_line = render_line;
    for (auto& wake : state->wake)
        if (!wake.init("raster_wake")) return false;
    g_state = state;
    for (unsigned i = 0; i < kWorkers; ++i) {
#if defined(__vita__)
        static const int cores[kWorkers] = {SCE_KERNEL_CPU_MASK_USER_1,
                                            SCE_KERNEL_CPU_MASK_USER_2};
        const SceUID thread = sceKernelCreateThread("raster", worker_entry, 0x10000100,
                                                    256 * 1024, 0, cores[i], nullptr);
        if (thread < 0 || sceKernelStartThread(thread, sizeof(i), &i) < 0) {
            g_state = nullptr; // a started peer only ever waits on its semaphore
            return false;
        }
#else
        std::thread(worker_loop, i).detach();
#endif
    }
    return true;
}

bool RasterWorkers::enabled() { return g_state != nullptr; }

void RasterWorkers::record_line(std::uint32_t y, std::uint16_t dispcnt,
                                const std::uint8_t* io, const std::uint8_t* vram,
                                const std::uint8_t* oam, const std::uint8_t* pal) {
    State& s = *g_state;
    if (y == 0) {
        s.recorded = 0;
        s.recording = free_job(s);
        if (s.recording) std::memcpy(s.recording->vram, vram, kVramBytes);
        else ++s.dropped;
    }
    if (!s.recording || y != s.recorded) return;
    Line& line = s.recording->lines[y];
    line.dispcnt = dispcnt;
    std::memcpy(line.io, io, sizeof(line.io));
    std::memcpy(line.oam, oam, sizeof(line.oam));
    std::memcpy(line.pal, pal, sizeof(line.pal));
    ++s.recorded;
}

bool RasterWorkers::frame_boundary(std::uint8_t* out) {
    State& s = *g_state;
    Job* newest = nullptr;
    // Both workers take jobs in submission order, so collection is in order.
    while (s.collected != s.submitted) {
        Job* job = s.ring[s.collected % s.ring.size()];
        if (job->remaining.load(std::memory_order_acquire) != 0) break;
        newest = job;
        ++s.collected;
        ++s.rendered;
    }
    if (newest) std::memcpy(out, newest->rgb, kRgbBytes);
    if (s.recording && s.recorded == kLines) {
        s.recording->remaining.store(kWorkers, std::memory_order_relaxed);
        s.ring[s.submitted % s.ring.size()] = s.recording;
        ++s.submitted;
        for (auto& wake : s.wake) wake.post();
    }
    s.recording = nullptr;
    s.recorded = 0;
#if !defined(__vita__)
    // Test mode: no lag and no drops, so output can be compared exactly.
    static const bool sync = std::getenv("GBARECOMP_RASTER_SYNC") != nullptr;
    if (sync && s.collected != s.submitted) {
        Job* job = s.ring[s.collected % s.ring.size()];
        while (job->remaining.load(std::memory_order_acquire) != 0) std::this_thread::yield();
        ++s.collected;
        ++s.rendered;
        std::memcpy(out, job->rgb, kRgbBytes);
        return true;
    }
#endif
    return newest != nullptr;
}

std::uint32_t RasterWorkers::frames_rendered() { return g_state ? g_state->rendered : 0; }
std::uint32_t RasterWorkers::frames_dropped() { return g_state ? g_state->dropped : 0; }

} // namespace gba
