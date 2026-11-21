# MCU-07B: 8052 linked-RAM gate (not a hardware test)

The target is the 8052-compatible STC89C52, with 128 bytes directly
addressable RAM and 256 bytes indirectly addressable internal RAM.
Only the **linked layout** is checked. No physical stack high-water,
UART timing, emergency-stop, flash, or human-facing safety is certified.

Run after a clean SDCC build:

`make -C firmware PLATFORM=8051
python3 -m unittest discover -s tests -p 'test_sdcc_areas.py' -v
python3 scripts/check_sdcc_areas.py firmware/build/firmware.map --mem firmware/build/firmware.mem`

The script fails when an area is missing/duplicated/malformed; when a
DSEG/OSEG/ISEG/SSEG address bound is exceeded; when OSEG/ISEG or ISEG/SSEG
intervals collide; or when the SDCC .mem stack and contiguous-free summary
disagrees with the .map. The reported SSEG size is a **static reserved
interval**, not measured worst-case stack use.

The current baseline has DSEG=127 B, OSEG=11 B (overlay; not additive
to DSEG), ISEG=59 B starting at 0x69, and a static SSEG interval of
92 B from 0xA4 to 0xFF. The .mem report shows **one byte** as the
largest contiguous spare internal RAM interval. DSEG and ISEG can
numerically overlap in the .map without physically colliding: consult
the .mem bitmap, do not sum linker areas naïvely.

The CI gate checks bounds and cross-report consistency, **not a minimum
engineering headroom**. A stricter minimum can be explicitly supplied as
`--min-largest-spare-bytes N` only after target-chip resource
budget review; no arbitrary threshold is imposed by default.

Fixture provenance: the test map area lines and .mem snapshot were
extracted from the SDCC 4.2.0 firmware build artifact published by
GitHub Actions PR #10, run 44446446525, head
75e2f6181189e30e5dee0f6ac18a479ef9c6b566. The stored .map fixture
contains the **area-table excerpt**, not the entire map. CI parses its
own full freshly built .map/.mem pair.

Rollback: revert the isolated MCU-07B commit in the PR branch; do not
rewrite main. Real-board stack measurements remain a separately
approved hardware acceptance activity.
