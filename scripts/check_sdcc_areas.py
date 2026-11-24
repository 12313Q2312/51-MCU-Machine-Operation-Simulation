#!/usr/bin/env python3
"""Verify 8052-class SDCC linked RAM area bounds against .map AND .mem.

Not a runtime stack high-water test, hardware qualification, or proof of a
comfortable direct-RAM margin. DSEG is an SDCC linker area, not a physical
occupancy bitmap: its numeric span can overlap ISEG in valid .map output.
"""

import argparse
import pathlib
import re
import sys

AREA_NAMES = ("DSEG", "OSEG", "ISEG", "SSEG")
AREA_LINE = re.compile(
    r"^\s*(DSEG|OSEG|ISEG|SSEG)\s+([0-9A-Fa-f]{4,8})\s+"
    r"([0-9A-Fa-f]{4,8})\s*=\s*(\d+)\.\s+bytes\s+\([^)]+\)\s*$",
    re.MULTILINE,
)
STACK_LINE = re.compile(
    r"Stack starts at:\s*0x([0-9A-Fa-f]+)\s*"
    r"\(sp set to 0x([0-9A-Fa-f]+)\)\s*"
    r"with\s*(\d+)\s+bytes available\.",
    re.IGNORECASE,
)
SPARE_LINE = re.compile(
    r"The largest spare internal RAM space starts at\s*0x([0-9A-Fa-f]+)\s*"
    r"with\s*(\d+)\s+bytes? available\.",
    re.IGNORECASE,
)


def check_map(text, *, max_dseg_end=0x80, max_iseg_end=0x100):
    """Return {area: (start, length)} after checking linked address bounds.

    Do not reject DSEG/ISEG numeric overlap: the linker's DSEG range is not
    a physical occupancy map (consult the actual .mem bitmap instead).
    """
    rows = {}
    for name, start_text, size_text, decimal_text in AREA_LINE.findall(text):
        if name in rows:
            raise ValueError(f"duplicate {name} area in linker report")
        start, size = int(start_text, 16), int(size_text, 16)
        if int(decimal_text) != size:
            raise ValueError(f"{name} hex and decimal sizes disagree")
        rows[name] = (start, size)

    for name in AREA_NAMES:
        # Ignore symbol assignments such as "ISEG = 0x0000" elsewhere
        # in a full SDCC map, while detecting malformed area table rows.
        occurrences = len(re.findall(
            r"^\s*" + name + r"\s+[0-9A-Fa-f]{4,8}\s+[0-9A-Fa-f]{4,8}\s*=",
            text, re.MULTILINE,
        ))
        if occurrences != 1 or name not in rows:
            raise ValueError(f"expected exactly one valid {name} area; found {occurrences}")

    for name, limit in (("DSEG", max_dseg_end), ("OSEG", max_dseg_end),
                        ("ISEG", max_iseg_end), ("SSEG", max_iseg_end)):
        start, size = rows[name]
        if size <= 0:
            raise ValueError(f"{name} must have positive size")
        if start + size > limit:
            raise ValueError(f"{name} end 0x{start + size:X} exceeds 0x{limit:X}")

    # OSEG and DSEG can overlap by design; do not add their lengths.
    oseg_start, oseg_size = rows["OSEG"]
    iseg_start, iseg_size = rows["ISEG"]
    sseg_start, sseg_size = rows["SSEG"]
    if oseg_start + oseg_size > iseg_start:
        raise ValueError("OSEG overlaps ISEG physical RAM")
    if iseg_start + iseg_size > sseg_start:
        raise ValueError("ISEG overlaps reserved SSEG stack interval")
    if sseg_start + sseg_size != max_iseg_end:
        raise ValueError("SSEG interval does not reach 8052 internal RAM end")
    return rows


def check_reports(map_text, mem_text, *, min_largest_spare_bytes=0):
    """Cross-check linker map with SDCC internal-RAM report, fail closed."""
    if min_largest_spare_bytes < 0:
        raise ValueError("minimum spare must be non-negative")
    areas = check_map(map_text)
    if "Internal RAM layout:" not in mem_text:
        raise ValueError(".mem internal RAM layout missing")

    stacks = STACK_LINE.findall(mem_text)
    spares = SPARE_LINE.findall(mem_text)
    if len(stacks) != 1 or len(spares) != 1:
        raise ValueError(".mem must contain exactly one stack and spare-space summary")
    stack_start, sp_initial, available = (int(stacks[0][0], 16),
                                           int(stacks[0][1], 16), int(stacks[0][2]))
    spare_start, spare_size = int(spares[0][0], 16), int(spares[0][1])
    sseg_start, sseg_size = areas["SSEG"]
    if stack_start != sseg_start or available != sseg_size or sp_initial + 1 != stack_start:
        raise ValueError(".mem stack reservation disagrees with .map SSEG")
    if spare_start + spare_size > 0x80:
        raise ValueError(".mem largest spare interval is not in directly addressable RAM")
    if spare_size < min_largest_spare_bytes:
        raise ValueError(
            f"largest spare internal RAM interval {spare_size} B is below budget "
            f"{min_largest_spare_bytes} B"
        )
    return areas, (spare_start, spare_size)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("map", type=pathlib.Path, help="real SDCC linker .map file")
    parser.add_argument("--mem", required=True, type=pathlib.Path,
                        help="corresponding real SDCC .mem report")
    parser.add_argument("--min-largest-spare-bytes", type=int, default=0,
                        help="optional explicit contiguous spare-RAM budget; default 0")
    args = parser.parse_args(argv)
    try:
        areas, (spare_start, spare_size) = check_reports(
            args.map.read_text(encoding="utf-8", errors="replace"),
            args.mem.read_text(encoding="utf-8", errors="replace"),
            min_largest_spare_bytes=args.min_largest_spare_bytes,
        )
    except (OSError, ValueError) as exc:
        print(f"SDCC RAM check FAILED: {exc}", file=sys.stderr)
        return 1
    for name in AREA_NAMES:
        start, size = areas[name]
        print(f"{name}: start=0x{start:02X}, size={size}, end=0x{start + size:02X}")
    print(f"Largest contiguous spare internal RAM interval: {spare_size} B at 0x{spare_start:02X}")
    if spare_size <= 1:
        print("WARNING: directly addressable RAM has almost no room for growth")
    print("PASS: linker-area/report bounds only. No runtime stack or board validation.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
