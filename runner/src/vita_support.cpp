// vita_support.cpp — PS Vita process configuration for MinishCapRecomp.
//
// Compiled only for the Vita target (see CMakeLists.txt).
#include <cstddef>
#include <cstdio>
#include <malloc.h>
#include <psp2/power.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/cpu.h>

extern "C" {
// newlib heap: kept modest on purpose. The 35 MB program plus this heap must
// leave room for SDL/GXM GPU buffers (16 MiB parameter buffer etc.).
unsigned int _newlib_heap_size_user = 96u * 1024u * 1024u;
// Generated guest code is deeply nested; give the main thread a big stack.
unsigned int sceUserMainThreadStackSize = 4u * 1024u * 1024u;
extern volatile unsigned long long g_vita_present_count;
extern volatile unsigned long long g_vita_phase[7];
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

}  // namespace minish_vita
