#!/usr/bin/env python3
"""Fail-closed SDCC area-budget check for an 8052-class firmware build.

This checks linked area sizes, not runtime stack high-water marks or hardware.
The linker may place OSEG and DSEG at overlapping addresses; do not add their
sizes together. The direct-RAM *end address* is the critical constraint.
"""
import argparse
import pathlib
import re
import sys

AREA = re.compile(
    r"^\s*(DSEG|ISEG)\s+([0-9A-Fa-f]{4,8})\s+([0-9A-Fa-f]{4,8})\s*=",
    re.MULTILINE,
)


def check_map(text, *, max_dseg_end=0x80, max_iseg_end=0x100):
    rows = {}
    for name, start, size in AREA.findall(text):
        if name in rows:
            raise ValueError(f"duplicate {name} area; ambiguous linker report")
        rows[name] = (int(start, 16), int(size, 16))
    missing = {"DSEG", "ISEG"} - rows.keys()
    if missing:
        raise ValueError(f"cannot verify memory layout: missing {sorted(missing)}")
    for name, bound in (("DSEG", max_dseg_end), ("ISEG", max_iseg_end)):
        start, size = rows[name]
        end = start + size
        if end > bound:
            raise ValueError(f"{name} end 0x{end:X} exceeds 0x{bound:X}")
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("map", type=pathlib.Path)
    args = parser.parse_args()
    try:
        rows = check_map(args.map.read_text(encoding="utf-8", errors="replace"))
    except (OSError, ValueError) as exc:
        print(f"SDCC area check failed: {exc}", file=sys.stderr)
        return 1
    for name, (start, size) in sorted(rows.items()):
        print(f"{name}: start=0x{start:02X}, size={size}, end=0x{start + size:02X}")
    print("Area address bounds verified; NOT a stack-depth or board-safety test")
    return 0


if __name__ == "__main__":
    sys.exit(main())
