# V1 software freeze and hardware handoff (2026-11-18)

## Accepted at freeze
The PC simulator, protocol unit tests, and SDCC 8051 firmware build have automated test evidence. See [hardware acceptance](HARDWARE_ACCEPTANCE.md) and [firmware verification](firmware-sdcc-verification.md) for exact tools, target, commands and known constraints. These results **do not** establish real-board operation.

## Hardware blockers
- No confirmed STC89C52 board flashing, UART ACK, LED matrix, emergency-stop or stack high-water measurements.
- Direct-addressable RAM is near exhaustion; avoid adding variables without rechecking the SDCC memory map.
- A frame may finish processing around an emergency-stop transition. Treat this as a **safety-critical blocker before any motor/actuator connection**, not merely a documentation caveat.
- Stack usage estimated from assembly is not a measurement.
- Web simulator queue capacity and embedded queue capacity may differ.

## Reopening gate
1. Confirm board model, oscillator, power supply, pin mapping and SDCC version before flashing.
2. Execute the checked-in hardware acceptance procedure on an isolated LED-only board; capture firmware SHA-256, UART trace, LED behavior and reset results.
3. Measure stack high-water under interrupt and queue stress; reject the build if it overlaps live data.
4. Reproduce emergency-stop race conditions; require a design review and fix before attaching any actuator.
5. Attach raw logs, hardware photos, toolchain versions, failures and reviewer approval to a PR.

Until then, describe the project as **software-tested / hardware-unverified**. No actuator deployment is authorized.
