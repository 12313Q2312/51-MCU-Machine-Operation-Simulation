"""SDCC linked RAM budget: real 4.2.0 artifact excerpt plus fault cases.

The fixtures were transcribed from a PR #10 firmware CI artifact. They are
not synthetic area sizes. The test does not execute firmware or real hardware.
"""

import pathlib
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from check_sdcc_areas import check_map, check_reports, main

FIXTURES = ROOT / "tests" / "fixtures"
MAP = (FIXTURES / "sdcc_4_2_0_firmware_area_excerpt.map").read_text(encoding="utf-8")
MEM = (FIXTURES / "sdcc_4_2_0_firmware.mem").read_text(encoding="utf-8")


class AreaBudgetTests(unittest.TestCase):
    def test_real_linker_area_format_and_mem_agree(self):
        areas, spare = check_reports(MAP, MEM)
        self.assertEqual(areas["DSEG"], (0, 127))
        self.assertEqual(areas["OSEG"], (0x14, 11))
        self.assertEqual(areas["ISEG"], (0x69, 59))
        self.assertEqual(areas["SSEG"], (0xA4, 92))
        self.assertEqual(spare, (0x1F, 1))

    def test_actual_dseg_and_iseg_numeric_overlap_is_not_false_failure(self):
        areas = check_map(MAP)
        self.assertGreater(areas["DSEG"][0] + areas["DSEG"][1], areas["ISEG"][0])

    def test_direct_ram_overflow(self):
        with self.assertRaisesRegex(ValueError, "DSEG end"):
            check_map(MAP.replace("DSEG                                00000000    0000007F =         127.",
                                  "DSEG                                00000002    0000007F =         127."))

    def test_indirect_ram_overflow(self):
        with self.assertRaisesRegex(ValueError, "ISEG end"):
            check_map(MAP.replace("ISEG                                00000069    0000003B =          59.",
                                  "ISEG                                000000F0    0000003B =          59."))

    def test_stack_and_idata_collision(self):
        bad = MAP.replace("SSEG                                000000A4", "SSEG                                000000A0")
        with self.assertRaisesRegex(ValueError, "ISEG overlaps"):
            check_map(bad)

    def test_non_area_symbol_labels_are_ignored(self):
        self.assertEqual(check_map(MAP + "\nISEG = 0x0000\n")["ISEG"], (0x69, 59))

    def test_missing_area_fails_closed(self):
        with self.assertRaisesRegex(ValueError, "exactly one valid SSEG"):
            check_map(MAP.replace("SSEG                                ", "BAD_                                "))

    def test_duplicate_area_fails_closed(self):
        line = next(line for line in MAP.splitlines() if line.startswith("OSEG "))
        with self.assertRaisesRegex(ValueError, "duplicate OSEG"):
            check_map(MAP + line + "\n")

    def test_hex_decimal_disagreement_fails_closed(self):
        with self.assertRaisesRegex(ValueError, "hex and decimal"):
            check_map(MAP.replace("=         127. bytes", "=         126. bytes"))

    def test_mem_stack_disagreement_fails_closed(self):
        with self.assertRaisesRegex(ValueError, "disagrees"):
            check_reports(MAP, MEM.replace("with 92 bytes available.", "with 91 bytes available."))

    def test_missing_mem_summary_fails_closed(self):
        with self.assertRaisesRegex(ValueError, "summary"):
            check_reports(MAP, MEM.replace("The largest spare internal RAM space", "No spare RAM space"))

    def test_explicit_contiguous_free_budget(self):
        with self.assertRaisesRegex(ValueError, "below budget"):
            check_reports(MAP, MEM, min_largest_spare_bytes=2)
        self.assertEqual(check_reports(MAP, MEM, min_largest_spare_bytes=1)[1][1], 1)

    def test_cli_accepts_real_excerpt(self):
        self.assertEqual(main([str(FIXTURES / "sdcc_4_2_0_firmware_area_excerpt.map"),
                               "--mem", str(FIXTURES / "sdcc_4_2_0_firmware.mem")]), 0)


if __name__ == "__main__":
    unittest.main()
