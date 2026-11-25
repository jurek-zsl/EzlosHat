// main.cpp — MinishCapRecomp entry point.
//
// Every gbarecomp game binary takes BOTH a BIOS and a ROM at launch
// (see ../gbarecomp/PRINCIPLES.md "BIOS is sacred"). The CLI accepts:
//
//   MinishCapRecomp [--bios <path>] [--rom <path>] [game.toml]
//
// All three are optional on the command line; missing values are
// pulled from game.toml. Hashes are verified before any code runs.
//
// Windowed play first runs the recomp-ui pre-boot launcher (Dear ImGui;
// see game_launcher_boot.h + gbarecomp's launcher_seam.h) to pick the ROM/BIOS
// and tune settings; the launcher's choices become ordinary CLI args for
// run_game(). Headless/explicit invocations (--tcp/--steps/--frames/
// --no-window/--rom/GBARECOMP_NO_LAUNCHER) bypass it entirely.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "runtime.h"
#include "minish_extended_view.h"
#include "mods/minish_definitive14.h"

#if defined(GBAGAME_RECOMP_UI)
#include "game_launcher_boot.h"
#endif

#if defined(__vita__)
namespace minish_vita { void boost_clocks(); void start_monitor(); }
#endif

namespace {

void print_usage() {
    std::printf(
        "MinishCapRecomp [--bios <path>] [--rom <path>] "
        "[game.toml]\n"
        "\n"
        "Both BIOS and ROM are required (either via flags or via the\n"
        "[bios] / [rom] sections of game.toml). The runtime refuses\n"
        "to start unless both hash-verify.\n"
        "\n"
        "Default BIOS path: ../gbarecomp/bios/gba_bios.bin\n"
        "Default game config: game.toml (in CWD)\n"
        "\n"
        "Windowed play opens the pre-boot launcher first; --no-launcher\n"
        "skips it for one run, --launcher forces it past a persisted\n"
        "\"skip launcher\" setting.\n");
}

}  // namespace

int main(int argc, char** argv) {
    std::printf("MinishCapRecomp (Phase 2 scaffold: BIOS bring-up)\n");

#if defined(__vita__)
    // PS Vita has no command line: always load the user's own BIOS and ROM
    // from ux0:data/EzlosHat/ (never shipped inside the VPK).
    // A BIOS bundled inside the VPK (personal builds only) wins; otherwise
    // use the user's own file next to the ROM.
    const char* vita_bios = "ux0:data/EzlosHat/gba_bios.bin";
    if (FILE* bf = std::fopen("app0:gba_bios.bin", "rb")) {
        std::fclose(bf);
        vita_bios = "app0:gba_bios.bin";
    }
    // ROM: ux0:data/EzlosHat/rom.gba (the older name is still accepted).
    const char* vita_rom = "ux0:data/EzlosHat/rom.gba";
    if (FILE* rf = std::fopen(vita_rom, "rb")) {
        std::fclose(rf);
    } else {
        vita_rom = "ux0:data/EzlosHat/minishcap_usa.gba";
    }
    const char* vita_argv[] = {
        "MinishCapRecomp",
        "--bios", vita_bios,
        "--rom",  vita_rom,
        nullptr,
    };
    argc = 5;
    argv = const_cast<char**>(vita_argv);
    // The Vita has no console: write everything to a log file, unbuffered so
    // it survives a crash. Also opt out of the 256 MiB replay recorder.
    std::freopen("ux0:data/EzlosHat/log.txt", "w", stdout);
    std::freopen("ux0:data/EzlosHat/log.txt", "a", stderr);
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    setenv("GBARECOMP_DIAGNOSTICS", "0", 1);
    setenv("GBARECOMP_FRAME_PHASE", "ux0:data/EzlosHat/phase.csv", 1);
    std::printf("EzlosHat start\n");
    minish_vita::boost_clocks();
    minish_vita::start_monitor();
    for (const char* f : {vita_bios, vita_rom}) {
        FILE* fp = std::fopen(f, "rb");
        std::printf("check %s: %s\n", f, fp ? "found" : "MISSING");
        if (fp) std::fclose(fp);
    }
#endif

    // Light arg sniff so we can show a useful usage line. The real
    // parser lives in the runtime; we just want a friendly fail.
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 ||
            std::strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        }
    }

    // Built-in defaults so a standalone MinishCapRecomp.exe ships
    // without a sibling game.toml. The picker still validates against
    // these values; CLI / TOML can override.
    gbarecomp::RunOptions opts;
    opts.builtin_game_name  = "The Legend of Zelda: The Minish Cap";
    opts.builtin_rom_sha1   = "b4bd50e4131b027c334547b4524e2dbbd4227130";
    // CRC32 of the pinned USA ROM (same dump the SHA-1 above gates on).
    // The launcher's GAME card uses it for its "ROM verified" check; the
    // asset picker treats it as informational next to the SHA-1 gate.
    opts.builtin_rom_crc32  = 0xABCEBBB1u;
    opts.mod_game_id        = "minish-cap-us";
#if defined(__vita__)
    opts.config_dir = "ux0:data/EzlosHat";
#endif
    minish::configure_definitive14(opts);
    opts.mod_owns_adaptive_view = true;
    // The adaptive-view implementation remains game-owned, but its activation
    // is now authoritative in Mods rather than the generic Display settings.
    opts.max_resize_view_width = 480;
    opts.resize_driven_view    = true;
    opts.extended_view_init    = &minish::install_extended_view;
    opts.launcher_expose_widescreen = false;
    opts.launcher_expose_adaptive_view = false;
    opts.launcher_region    = "USA";
    opts.launcher_game_config = "game.toml";   // prefill ROM/BIOS from [rom]/[bios]
    // Save: game.toml [save] has no explicit path — the runtime derives
    // <rom>.sav, and the launcher seam shows the same derivation.

#if defined(GBAGAME_RECOMP_UI)
    std::vector<std::string> args(argv, argv + argc);
    if (game_launcher_preboot(args, opts)) return 0;   // user quit the launcher
    std::vector<char*> av;
    av.reserve(args.size());
    for (auto& s : args) av.push_back(s.data());
    return gbarecomp::run_game(static_cast<int>(av.size()), av.data(), opts);
#else
    return gbarecomp::run_game(argc, argv, opts);
#endif
}
