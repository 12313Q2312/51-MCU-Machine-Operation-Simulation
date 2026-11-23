"""Offline fault tests for the SDCC RAM declaration profiler.

The assembly examples come from the actual 4.2.0 firmware assembly artifact.
They do not prove hardware RAM safety or measure runtime stack depth.
"""
import pathlib
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from analyze_sdcc_ram import analyze, main, parse_asm, render
from check_sdcc_areas import check_reports

FIX = ROOT / "tests" / "fixtures"
MAP = (FIX / "sdcc_4_2_0_firmware_area_excerpt.map").read_text(encoding="utf-8")
MEM = (FIX / "sdcc_4_2_0_firmware.mem").read_text(encoding="utf-8")
MAIN = """
    .area DSEG (DATA)
_system_running:
    .ds 1
_tick_counter:
    .ds 4
_main_data_131074_40:
    .ds 7
    .area OSEG (OVR,DATA)
__delay_ms_i_131072_26:
    .ds 2
    .area ISEG (DATA)
_frame_queue:
    .ds 59
    .area CSEG (CODE)
_main:
    ret
"""
PROTOCOL = """
    .area DSEG (DATA)
_rx_buf:
    .ds 10
_rx_pos:
    .ds 1
_rx_synced:
    .ds 1
_protocol_feed_PARM_2:
    .ds 2
_protocol_feed_data_65536_17:
    .ds 7
    .area OSEG (OVR,DATA)
_protocol_decode_PARM_2:
    .ds 3
"""
QUEUE = """
    .area DSEG (DATA)
_queue_push_PARM_2:
    .ds 3
_queue_push_q_65536_12:
    .ds 3
"""
LED = """
    .area DSEG (DATA)
_framebuffer:
    .ds 8
_current_row:
    .ds 1
"""


class ProfileTests(unittest.TestCase):
    def test_main_declarations_match_compiled_size(self):
        rows = parse_asm("main.asm", MAIN)
        self.assertEqual(sum(r.size for r in rows if r.area == "DSEG"), 12)
        self.assertEqual(sum(r.size for r in rows if r.area == "ISEG"), 59)
        self.assertEqual(next(r.size for r in rows if r.symbol == "_tick_counter"), 4)

    def test_protocol_buffer_is_ten_bytes(self):
        rows = parse_asm("protocol.asm", PROTOCOL)
        self.assertEqual(next(r.size for r in rows if r.symbol == "_rx_buf"), 10)
        self.assertEqual(sum(r.size for r in rows if r.area == "DSEG"), 21)

    def test_non_ram_section_ignored(self):
        self.assertEqual(len(parse_asm("main.asm", MAIN)), 5)

    def test_unlabelled_reservation_fails(self):
        with self.assertRaisesRegex(ValueError, "unlabelled"):
            parse_asm("f.asm", ".area DSEG (DATA)\n.ds 4\n")

    def test_duplicate_fails(self):
        with self.assertRaisesRegex(ValueError, "duplicate"):
            parse_asm("f.asm", ".area DSEG (DATA)\n_a:\n.ds 1\n_a:\n.ds 1\n")

    def test_unsupported_expression_fails(self):
        with self.assertRaisesRegex(ValueError, "unsupported"):
            parse_asm("f.asm", ".area DSEG (DATA)\n_a:\n.ds CONSTANT\n")

    def test_no_ram_fails(self):
        with self.assertRaisesRegex(ValueError, "no RAM"):
            parse_asm("f.asm", ".area CSEG (CODE)\n_f:\nret\n")

    def test_valid_report_from_four_units(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = pathlib.Path(temp)
            sources = {"main.asm": MAIN, "protocol.asm": PROTOCOL,
                       "queue.asm": QUEUE, "led_matrix.asm": LED}
            for name, body in sources.items():
                (directory / name).write_text(body, encoding="utf-8")
            (directory / "firmware.map").write_text(MAP, encoding="utf-8")
            (directory / "firmware.mem").write_text(MEM, encoding="utf-8")
            output = directory / "source-profile.md"
            self.assertEqual(main([str(directory), "--output", str(output)]), 0)
            result = output.read_text(encoding="utf-8")
            self.assertIn("| main.asm | 12 | 59 | 2 |", result)
            self.assertIn("| main.asm | DSEG | _tick_counter | 4 |", result)
            self.assertIn("1 byte(s)", result)

    def test_missing_compilation_unit_fails_closed(self):
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaises(FileNotFoundError):
                analyze(pathlib.Path(temp))

    def test_overlays_not_called_additive(self):
        areas, spare = check_reports(MAP, MEM)
        report = render(parse_asm("main.asm", MAIN), areas, spare)
        self.assertIn("MUST NOT be added to DSEG", report)
        self.assertIn("NOT runtime stack high-water", report)


if __name__ == "__main__":
    unittest.main()
