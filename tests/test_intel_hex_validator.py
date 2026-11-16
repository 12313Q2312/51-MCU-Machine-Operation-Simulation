"""Self-tests for the Intel HEX validator used by the 8051 firmware CI."""
from pathlib import Path
import tempfile
import unittest

from scripts.validate_intel_hex import HexValidationError, validate_hex


def record(kind: int, data: bytes = b"", address: int = 0) -> str:
    payload = bytes([len(data), address >> 8, address & 255, kind]) + data
    return ":" + (payload + bytes([(-sum(payload)) & 255])).hex().upper()


class IntelHexTests(unittest.TestCase):
    def validate(self, lines: list[str]) -> tuple[int, int]:
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "image.hex"
            path.write_text("\n".join(lines) + "\n", encoding="ascii")
            return validate_hex(path)

    def test_valid_image(self):
        self.assertEqual(self.validate([record(0, b"\x75\x80\x00"), record(1)]), (2, 3))

    def test_valid_extended_address(self):
        self.assertEqual(self.validate([record(4, b"\x00\x01"), record(0, b"\x00"), record(1)]), (3, 1))

    def test_corrupted_checksum(self):
        with self.assertRaisesRegex(HexValidationError, "checksum"):
            self.validate([record(0, b"\x12")[:-2] + "00", record(1)])

    def test_missing_eof(self):
        with self.assertRaisesRegex(HexValidationError, "missing EOF"):
            self.validate([record(0, b"\x12")])

    def test_empty_firmware(self):
        with self.assertRaisesRegex(HexValidationError, "no firmware"):
            self.validate([record(1)])

    def test_record_after_eof(self):
        with self.assertRaisesRegex(HexValidationError, "after EOF"):
            self.validate([record(0, b"\x12"), record(1), record(0, b"\x34")])

    def test_malformed_length(self):
        with self.assertRaisesRegex(HexValidationError, "length"):
            self.validate([":0200000001FD", record(1)])


if __name__ == "__main__":
    unittest.main()
