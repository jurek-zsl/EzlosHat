#!/usr/bin/env bash
# Generate the C++ for YOUR OWN game and BIOS dumps. Output is local build data:
# it is derived from your files and must never be committed or shared.
#
#   runner/tools/generate.sh /path/to/rom.gba /path/to/gba_bios.bin
set -euo pipefail
ROM=${1:?usage: generate.sh ROM.gba BIOS.bin}
BIOS=${2:?usage: generate.sh ROM.gba BIOS.bin}
HERE=$(cd "$(dirname "$0")/.." && pwd)
FW="$HERE/../gbarecomp"
mkdir -p "$HERE/roms" "$FW/bios"
cp "$ROM"  "$HERE/roms/rom.gba"
cp "$BIOS" "$FW/bios/gba_bios.bin"
if [ ! -x "$FW/build/gba_recompile" ]; then
  cmake -S "$FW" -B "$FW/build" -G Ninja -DCMAKE_BUILD_TYPE=Release
  cmake --build "$FW/build" --target gba_recompile --parallel
fi
"$FW/build/gba_recompile" --bios "$FW/bios/gba_bios.bin" --config "$FW/bios/gba_bios.toml" \
  --out "$FW/src/runtime/generated_bios"
"$FW/build/gba_recompile" --rom "$HERE/roms/rom.gba" \
  --config "$HERE/symbols/minishcap.toml" --config "$HERE/symbols/minishcap_reviewed.toml" \
  --out "$HERE/generated"
echo "Generated. Now build the Vita package (see README)."
