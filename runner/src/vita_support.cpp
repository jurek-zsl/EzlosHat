// vita_support.cpp — PS Vita process configuration for MinishCapRecomp.
//
// Compiled only for the Vita target (see CMakeLists.txt).
#include <cstddef>
#include <cstdio>
#include <malloc.h>
#include <thread>
#include <chrono>

#include <psp2/power.h>
#include <psp2/kernel/sysmem.h>

extern "C" {
// newlib heap: kept modest on purpose. The 35 MB program plus this heap must
// leave room for SDL/GXM GPU buffers (16 MiB parameter buffer etc.).
unsigned int _newlib_heap_size_user = 96u * 1024u * 1024u;
// Generated guest code is deeply nested; give the main thread a big stack.
unsigned int sceUserMainThreadStackSize = 4u * 1024u * 1024u;
extern volatile unsigned long long g_vita_present_count;
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

// Once per 2 s: frames presented, heap in use, free system memory. Lets the
// log show whether slowdowns coincide with memory growth.
void start_monitor() {
    std::thread([] {
        unsigned long long last = 0;
        int tick = 0;
        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            const unsigned long long now = g_vita_present_count;
            struct mallinfo mi = mallinfo();
            std::fprintf(stderr, "perf t=%ds fps=%.1f heap_used=%.1fMB heap_arena=%.1fMB\n",
                         ++tick * 2, (now - last) / 2.0, mi.uordblks / 1048576.0,
                         mi.arena / 1048576.0);
            last = now;
        }
    }).detach();
}

}  // namespace minish_vita
