"""Synthetic linker-map parser tests; not a substitute for real SDCC output."""
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "scripts"))
from check_sdcc_areas import check_map


class AreaBudgetTests(unittest.TestCase):
    def test_valid_8052_layout(self):
        report = "DSEG 00000000 0000007F = 127. bytes\nISEG 00000069 0000003B = 59. bytes"
        areas = check_map(report)
        self.assertEqual(areas["DSEG"], (0, 127))
        self.assertEqual(areas["ISEG"], (0x69, 59))

    def test_direct_ram_overflow(self):
        with self.assertRaisesRegex(ValueError, "DSEG end"):
            check_map("DSEG 00000002 0000007F = 127. bytes\nISEG 00000069 0000003B = 59. bytes")

    def test_indirect_ram_overflow(self):
        with self.assertRaisesRegex(ValueError, "ISEG end"):
            check_map("DSEG 00000000 0000007F = 127. bytes\nISEG 000000F0 00000020 = 32. bytes")

    def test_missing_area_fails_closed(self):
        with self.assertRaisesRegex(ValueError, "missing"):
            check_map("DSEG 00000000 0000007F = 127. bytes")

    def test_duplicate_area_fails_closed(self):
        with self.assertRaisesRegex(ValueError, "duplicate"):
            check_map("DSEG 00000000 0000007F = 127. bytes\nDSEG 00000000 0000007F = 127. bytes\nISEG 00000069 0000003B = 59. bytes")


if __name__ == "__main__":
    unittest.main()
