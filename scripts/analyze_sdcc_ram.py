#!/usr/bin/env python3
"""Inspect SDCC per-module RAM declarations; not a physical RAM measurement.

Source .asm storage requests are not additive with overlaid areas.
Actual linker address bounds and static stack reservation come from .map/.mem.
"""
import argparse
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
import re
import sys

from check_sdcc_areas import check_reports

AREAS = ("DSEG", "ISEG", "OSEG")
AREA = re.compile(r"^\s*\.area\s+([A-Za-z_]\w*)\b")
LABEL = re.compile(r"^\s*([A-Za-z_.$][\w.$]*):\s*$")
RESERVE = re.compile(r"^\s*\.ds\s+([0-9]+|0[xX][0-9a-fA-F]+)\s*$")
SOURCE_FILES = ("main.asm", "protocol.asm", "queue.asm", "led_matrix.asm")


@dataclass(frozen=True)
class Declaration:
    source: str
    area: str
    symbol: str
    size: int


def parse_asm(source, text):
    area = None
    pending = None
    rows = []
    seen = set()
    for number, raw in enumerate(text.splitlines(), 1):
        line = raw.split(";", 1)[0].strip()
        if not line:
            continue
        match = AREA.match(line)
        if match:
            area = match.group(1)
            pending = None
            continue
        if area not in AREAS:
            continue
        match = LABEL.fullmatch(line)
        if match:
            pending = match.group(1)
            continue
        match = RESERVE.fullmatch(line)
        if match:
            if pending is None:
                raise ValueError(f"{source}:{number}: unlabelled .ds")
            token = match.group(1)
            size = int(token, 16) if token.lower().startswith("0x") else int(token)
            if size <= 0:
                raise ValueError(f"{source}:{number}: invalid reservation")
            if (area, pending) in seen:
                raise ValueError(f"{source}:{number}: duplicate symbol {pending}")
            seen.add((area, pending))
            rows.append(Declaration(source, area, pending, size))
            pending = None
        elif line.startswith(".ds"):
            raise ValueError(f"{source}:{number}: unsupported .ds syntax")
    if not rows:
        raise ValueError(f"{source}: no RAM declarations")
    return rows


def analyze(directory):
    rows = []
    for filename in SOURCE_FILES:
        rows.extend(parse_asm(filename, (directory / filename).read_text(encoding="utf-8")))
    return rows


def render(rows, areas, spare):
    totals = defaultdict(int)
    for row in rows:
        totals[(row.source, row.area)] += row.size
    out = [
        "# 8052 SDCC source-level RAM report",
        "",
        "Physical placement is defined by linked .map/.mem, not these .asm declarations.",
        "OSEG is overlaid and MUST NOT be added to DSEG.",
        "SSEG is the static stack reservation, NOT runtime stack high-water.",
        "",
        "## Linked areas (bounds checked by MCU-07B)",
        "",
        "| Area | Start | Size (bytes) |",
        "|---|---:|---:|",
    ]
    for name in ("DSEG", "OSEG", "ISEG", "SSEG"):
        start, size = areas[name]
        out.append(f"| {name} | 0x{start:02X} | {size} |")
    out += [
        "",
        f"Largest contiguous spare internal RAM: {spare[1]} byte(s) at 0x{spare[0]:02X}.",
        "",
        "## Per compiled C module declarations",
        "",
        "| Module | DSEG .ds | ISEG .ds | OSEG .ds |",
        "|---|---:|---:|---:|",
    ]
    for name in SOURCE_FILES:
        out.append(
            f"| {name} | {totals[(name, 'DSEG')]} | "
            f"{totals[(name, 'ISEG')]} | {totals[(name, 'OSEG')]} |"
        )
    out += [
        "",
        "## Top storage requests (not a physical address map)",
        "",
        "| Module | Area | Symbol | Bytes |",
        "|---|---|---|---:|",
    ]
    for row in sorted(rows, key=lambda r: (-r.size, r.source, r.symbol))[:20]:
        out.append(f"| {row.source} | {row.area} | {row.symbol} | {row.size} |")
    out += [
        "",
        "No physical MCU, UART, ISR timing or stack high-water measurement was performed.",
        "",
    ]
    return "\n".join(out)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build_dir", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    try:
        rows = analyze(args.build_dir)
        areas, spare = check_reports(
            (args.build_dir / "firmware.map").read_text(encoding="utf-8", errors="replace"),
            (args.build_dir / "firmware.mem").read_text(encoding="utf-8", errors="replace"),
        )
        report = render(rows, areas, spare)
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(report, encoding="utf-8")
        else:
            print(report)
        return 0
    except (OSError, ValueError) as exc:
        print(f"RAM profile failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
