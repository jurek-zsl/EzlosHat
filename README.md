# Ezlo's Hat

An experimental **PlayStation Vita** runner for a Game Boy Advance action-adventure game,
built on a static-recompilation framework. It translates the machine code of a game
**you already own** into native code that runs on the Vita, at full screen and with
Vita controls.

> **This project contains no game data.** It does not include, and will never include,
> a ROM, a BIOS, artwork, audio, or any other copyrighted game content. You must supply
> your own legally obtained files, dumped from hardware you own.

This is an unofficial, fan-made, non-commercial project. It is not affiliated with,
endorsed by, or sponsored by any game publisher or console manufacturer. All trademarks
belong to their respective owners.

## Status

Early and experimental. It boots, plays, and saves on real hardware, but performance and
polish are still being worked on. Expect rough edges.

| Area | State |
|---|---|
| Boot, gameplay, save files | Working |
| Full-screen stretched video | Working |
| Frame rate | Being optimised (see Known issues) |
| In-game config menu | Planned |
| Audio, touch / rear-touch / gyro | Not yet verified on Vita |

## What you need

- A PS Vita running custom firmware (HENkaku / taiHEN), to install homebrew.
- Your own dump of the game ROM (a clean, unmodified image, identified by SHA-1
  `b4bd50e4131b027c334547b4524e2dbbd4227130`).
- Your own dump of the console BIOS (16 KiB, SHA-1 `300c20df6731a33952ded8c436f7f186d25d3492`).
- A Mac or Linux computer with [VitaSDK](https://vitasdk.org/) to build.

The runner refuses to start unless both files match those hashes.

## Why you have to build it yourself

The recompiler turns *your* ROM and BIOS into C++ source, which is then compiled into the
Vita program. That generated code is derived from your copies, so it is local build output:
it is never committed here, and the resulting `.vpk` should **not** be shared or uploaded.
This repository only contains the tools and the Vita platform layer.

## Build

1. Install VitaSDK (with `sdl2` and `vitaGL` via `vdpm`), CMake and Ninja.
2. Generate the code from your own files (this also builds the host recompiler):

   ```sh
   runner/tools/generate.sh /path/to/your/rom.gba /path/to/your/gba_bios.bin
   ```

   Your files are copied into `runner/roms/` and `gbarecomp/bios/`, both git-ignored.

3. Cross-compile for the Vita:

   ```sh
   cd runner
   cmake -S . -B build-vita -G Ninja \
     -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake \
     -DCMAKE_BUILD_TYPE=Release \
     -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG" -DCMAKE_C_FLAGS_RELEASE="-O2 -DNDEBUG" \
     -DGBARECOMP_ROOT=../gbarecomp -DEZLO_ENABLE_DE14=OFF -DGBAGAME_RECOMP_UI=OFF
   cmake --build build-vita --target EzlosHat.vpk -j7
   ```

   The first build compiles roughly 270 MB of generated code and takes a while.
   To use your own LiveArea artwork, put it in `runner/vita/local/sce_sys/` (git-ignored);
   otherwise neutral placeholder art is used. For a **personal** package you may also
   pass `-DEZLO_VITA_BIOS=/path/to/gba_bios.bin` to bundle your BIOS into your own
   `.vpk`. Never share a package built that way.

## Install and run

1. Copy `EzlosHat.vpk` to the Vita and install it with VitaShell.
2. Create `ux0:data/EzlosHat/` and copy in your ROM as `rom.gba`, plus your BIOS as
   `gba_bios.bin` (unless you built the BIOS into your own personal package).
3. Launch **Ezlo's Hat** from the LiveArea. Saves are written next to the ROM.

A log is written to `ux0:data/EzlosHat/log.txt` on every run.

## Known issues

- The frame rate can drop well below full speed; the runner logs fps and memory use
  every two seconds to help track this down.
- Self-healing code paths and the mod system are disabled on the Vita for now.

## Credits

This project builds on the work of others; see [NOTICE.md](NOTICE.md) for authorship,
upstream sources and licenses.

## License

[PolyForm Noncommercial 1.0.0](LICENSE). Non-commercial use only.
