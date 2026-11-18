#!/usr/bin/env python3
"""Validate Intel HEX record structure and checksums; not hardware functionality."""
from pathlib import Path
import argparse


class HexValidationError(ValueError):
    pass


def validate_hex(path: Path) -> tuple[int, int]:
    records = 0
    data_bytes = 0
    eof = False
    for line_number, line in enumerate(path.read_text(encoding="ascii").splitlines(), 1):
        if not line.startswith(":"):
            raise HexValidationError(f"line {line_number}: missing ':'")
        try:
            raw = bytes.fromhex(line[1:])
        except ValueError as exc:
            raise HexValidationError(f"line {line_number}: invalid hex") from exc
        if len(raw) < 5 or len(raw) != raw[0] + 5:
            raise HexValidationError(f"line {line_number}: incorrect record length")
        if sum(raw) % 256:
            raise HexValidationError(f"line {line_number}: checksum mismatch")
        if eof:
            raise HexValidationError(f"line {line_number}: record after EOF")
        byte_count = raw[0]
        address = (raw[1] << 8) | raw[2]
        kind = raw[3]
        if kind == 0:
            data_bytes += byte_count
        elif kind == 1:
            if byte_count or address:
                raise HexValidationError(f"line {line_number}: malformed EOF")
            eof = True
        elif kind in (2, 4):
            if byte_count != 2 or address:
                raise HexValidationError(f"line {line_number}: malformed address extension")
        else:
            raise HexValidationError(f"line {line_number}: unsupported record type {kind}")
        records += 1
    if not eof:
        raise HexValidationError("missing EOF record")
    if data_bytes == 0:
        raise HexValidationError("no firmware data records")
    return records, data_bytes


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("hex_path", type=Path)
    args = parser.parse_args()
    try:
        records, size = validate_hex(args.hex_path)
    except (HexValidationError, OSError, UnicodeError) as exc:
        parser.exit(1, f"Invalid Intel HEX: {exc}\n")
    print(f"Valid Intel HEX: {records} records, {size} data bytes")


if __name__ == "__main__":
    main()
