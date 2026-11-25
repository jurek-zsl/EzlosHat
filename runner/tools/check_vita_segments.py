#!/usr/bin/env python3
"""Fail if the gap after the code segment is too small for vita-elf-create."""
import subprocess, sys
NEED = 3300
out = subprocess.run([sys.argv[1], "-lW", sys.argv[2]], capture_output=True, text=True, check=True).stdout
loads = [l.split() for l in out.splitlines() if l.split()[:1] == ["LOAD"]]
end = int(loads[0][2], 16) + int(loads[0][4], 16)
gap = int(loads[1][2], 16) - end
if gap < NEED:
    # Padding of d bytes changes the gap to (gap - d) mod 4096; aim for ~3584.
    sys.exit(f"segment gap {gap} < {NEED}: add {(gap - 3584) % 4096} to "
             "EZLO_SEGMENT_PAD (mod 4096) in src/vita_support.cpp")
print(f"segment gap {gap} ok")
