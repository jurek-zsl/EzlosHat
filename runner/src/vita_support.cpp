// vita_support.cpp — PS Vita process configuration for MinishCapRecomp.
//
// Compiled only for the Vita target (see CMakeLists.txt).
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <malloc.h>
#include <psp2/power.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/cpu.h>
#include <psp2/kernel/processmgr.h>

extern "C" {
// newlib heap: kept modest on purpose. The 35 MB program plus this heap must
// leave room for SDL/GXM GPU buffers (16 MiB parameter buffer etc.).
unsigned int _newlib_heap_size_user = 96u * 1024u * 1024u;
// Generated guest code is deeply nested; give the main thread a big stack.
unsigned int sceUserMainThreadStackSize = 4u * 1024u * 1024u;
extern volatile unsigned long long g_vita_present_count;
// vita-elf-create needs ~3.3 KB free between the code and data segments.
// Adjust EZLO_SEGMENT_PAD (bytes, 0-4095) if the post-link check fails.
#ifndef EZLO_SEGMENT_PAD
#define EZLO_SEGMENT_PAD 2048
#endif
__attribute__((used)) const unsigned char ezlo_segment_pad[EZLO_SEGMENT_PAD] = {1};
extern volatile unsigned long long g_vita_phase[7];
extern unsigned long long g_vita_ticks;
extern unsigned long long g_prof_scanline_ns, g_prof_scanline_count;
#if defined(GBARECOMP_DETAILED_PHASE_PROFILE)
extern unsigned long long g_prof_device_ns[6], g_prof_device_count[6];
extern unsigned long long dispatch_resolve_ns, dispatch_resolve_calls;
#endif
}

namespace minish_vita {

// Maximum clocks: the default 333 MHz CPU is too slow for recompiled code.
void boost_clocks() {
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);
    std::printf("clocks: arm=%d bus=%d gpu=%d xbar=%d MHz\n",
                scePowerGetArmClockFrequency(), scePowerGetBusClockFrequency(),
                scePowerGetGpuClockFrequency(), scePowerGetGpuXbarClockFrequency());
}

// Once per 2 s: frames presented and heap in use. Lets the log show whether
// slowdowns coincide with memory growth. Uses a native Vita thread because
// std::thread / pthread_create fail at run time on this platform.
static int monitor_main(SceSize, void*) {
    unsigned long long last = 0;
    unsigned long long prev_phase[7] = {0, 0, 0, 0, 0, 0, 0};
    unsigned long long prev_chunks = 0, prev_ticks = 0, prev_dev[6] = {0, 0, 0, 0, 0, 0}, prev_disp_ns = 0, prev_disp = 0, prev_line_ns = 0;
    int tick = 0;
    for (;;) {
        sceKernelDelayThread(2 * 1000 * 1000);
        const unsigned long long now = g_vita_present_count;
        struct mallinfo mi = mallinfo();
        std::fprintf(stderr, "perf t=%ds fps=%.1f heap_used=%.1fMB heap_arena=%.1fMB\n",
                     ++tick * 2, (now - last) / 2.0, mi.uordblks / 1048576.0,
                     mi.arena / 1048576.0);
        last = now;
        // Average milliseconds per frame spent in each phase (last 2 s).
        unsigned long long cur[7], d[7];
        for (int i = 0; i < 7; ++i) { cur[i] = g_vita_phase[i]; d[i] = cur[i] - prev_phase[i]; prev_phase[i] = cur[i]; }
        if (d[6] > 0)
            std::fprintf(stderr,
                "phase ms/frame (render=on workers): guest=%.2f render=%.2f present=%.2f audio=%.2f pump=%.2f pacer_wait=%.2f (frames=%llu)\n",
                d[0] / 1000.0 / d[6], d[1] / 1000.0 / d[6], d[2] / 1000.0 / d[6],
                d[3] / 1000.0 / d[6], d[4] / 1000.0 / d[6], d[5] / 1000.0 / d[6], d[6]);
        const unsigned long long ticks = g_vita_ticks;
        if (d[6] > 0) {
            std::fprintf(stderr, "guest insns/frame=%llu",
                         (ticks - prev_ticks) / d[6]);
#if defined(GBARECOMP_DETAILED_PHASE_PROFILE)
            static const char* names[6] = {"audio", "timers", "serial", "ppu", "hdma", "vdma"};
            for (int i = 0; i < 6; ++i) {
                std::fprintf(stderr, " %s=%.2fms", names[i],
                             (g_prof_device_ns[i] - prev_dev[i]) / 1e6 / d[6]);
                prev_dev[i] = g_prof_device_ns[i];
            }
            std::fprintf(stderr, " device_chunks/frame=%llu",
                         (g_prof_device_count[0] - prev_chunks) / d[6]);
            prev_chunks = g_prof_device_count[0];
            std::fprintf(stderr, " scanline_record=%.2fms dispatch=%.2fms/%llu",
                         (g_prof_scanline_ns - prev_line_ns) / 1e6 / d[6],
                         (dispatch_resolve_ns - prev_disp_ns) / 1e6 / d[6],
                         (dispatch_resolve_calls - prev_disp) / d[6]);
            prev_line_ns = g_prof_scanline_ns;
            prev_disp_ns = dispatch_resolve_ns; prev_disp = dispatch_resolve_calls;
#endif
            std::fprintf(stderr, "\n");
        }
        prev_ticks = ticks;
    }
    return 0;
}

void pin_main_thread() {
    sceKernelChangeThreadCpuAffinityMask(0, SCE_KERNEL_CPU_MASK_USER_0);
}

void start_monitor() {
    SceUID t = sceKernelCreateThread("ezlos_perf", monitor_main, 0x10000100, 0x10000, 0, 0, nullptr);
    if (t >= 0) sceKernelStartThread(t, 0, nullptr);
}

// One-time micro-benchmark of the per-instruction helpers, run from the game
// thread once gameplay is live. Prints nanoseconds per call.
extern "C" {
bool runtime_should_yield(void);
void runtime_tick(uint32_t cycles);
void arm_set_nzcv_sub(uint32_t a, uint32_t b, uint32_t r);
uint32_t bus_read_u32(uint32_t addr);
uint32_t runtime_mem_cycles(uint32_t addr, uint32_t width, uint32_t sequential);
}
__attribute__((noinline)) static unsigned ezlo_empty(unsigned v) { __asm__ volatile(""); return v + 1; }
extern "C" void ezlo_microbench() {
    const int n = 100000;
    auto time = [&](const char* name, auto&& body) {
        const SceUInt64 t0 = sceKernelGetProcessTimeWide();
        for (int i = 0; i < n; ++i) body(i);
        const SceUInt64 t1 = sceKernelGetProcessTimeWide();
        std::fprintf(stderr, "bench %-14s %.1f ns/call\n", name, (t1 - t0) * 1000.0 / n);
    };
    volatile unsigned sink = 0;
    time("empty_call", [&](int i) { sink = ezlo_empty(i); });
    time("should_yield", [&](int) { sink = runtime_should_yield(); });
    time("tick_0", [&](int) { runtime_tick(0); });
    time("set_nzcv_sub", [&](int i) { arm_set_nzcv_sub(i, 3, i - 3); });
    time("read_ewram", [&](int i) { sink = bus_read_u32(0x02000000u + (i & 0xFFC)); });
    time("mem_cycles", [&](int i) { sink = runtime_mem_cycles(0x02000000u + (i & 0xFFC), 4, 0); });
}

}  // namespace minish_vita
