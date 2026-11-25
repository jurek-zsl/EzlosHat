# Notice and credits

This repository combines and modifies work from other authors. Their licenses and
required notices are preserved here.

## Upstream projects

| Component | Author | Upstream | Commit used |
|---|---|---|---|
| `gbarecomp/` — GBA static-recompilation framework and runtime | Matthew Stanley | https://github.com/mstan/gbarecomp | `62b6a944089b0bf8c9c699baab2c6865e9ddf1c8` |
| `runner/` — game runner (upstream directory name differs), symbol metadata, and mod glue | Matthew Stanley | https://github.com/mstan/MinishCapRecomp | `cb2525b622d6428f0a10167aef089417b30e8631` |

Required Notice: Copyright (c) 2026 Matthew Stanley.

Both are licensed under the
[PolyForm Noncommercial License 1.0.0](https://polyformproject.org/licenses/noncommercial/1.0.0)
(see `LICENSE`, `gbarecomp/LICENSE`, `runner/LICENSE`). The separate grant in
`runner/PERMISSIONS.md` also applies to the code it covers. Third-party
materials inside those trees keep their own terms (see `gbarecomp/THIRD_PARTY_ATTRIBUTION.md`).
The symbol metadata in `runner/symbols/` was produced from a community
decompilation project's function names and addresses; no source code from it is included.

Vendored submodules (kept in `gbarecomp/external/`) retain their own licenses.

## Changes made in this repository

- Added a PlayStation Vita platform layer: process configuration, entry point,
  full-screen stretched presentation, clock boost, file logging, and VPK packaging.
- Disabled runtime overlay compilation on the Vita (no `dlopen` / host compiler there).
- Made the optional enhancement-mod package optional in the build.
- Fixed linking on macOS and non-PIC builds for the Vita.

All of these modifications are released under the same PolyForm Noncommercial license.

## Not included

No game ROM, BIOS image, game artwork, audio or other game content is included, and none
of the generated code derived from them. The BIOS-derived files
`gbarecomp/src/runtime/generated_bios/bios_recompiled.*` and `bios_symbol_map.cpp` are
git-ignored.

## Tools

Built with [VitaSDK](https://vitasdk.org/), SDL2 and vitaGL.
