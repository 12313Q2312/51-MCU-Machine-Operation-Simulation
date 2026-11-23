# MCU-07C: static RAM attribution and optimization candidate

**Scope:** diagnosis only. No firmware C source, UART protocol, real hardware behavior, or main branch changes. The aim is to make storage allocations reviewable before considering optimization.

## Reproducible evidence

The baseline is main commit `470514cfb65f22843b0fe69d4953b6f8bf6f9f9e`. The frozen assembly evidence is from the SDCC 4.2.0 firmware artifact of [PR #10 run 44446446525](https://github.com/12313Q2312/51-MCU-Machine-Operation-Simulation/actions/runs/44446446525), source head `75e2f6181189e30e5dee0f6ac18a479ef9c6b566`. The production firmware C files on this head match main. PR #10 is the separate, still-unmerged linked-area safety gate; this Draft PR is stacked on its branch to avoid duplicating or conflicting with that pending change.

The linker reports DSEG 127 B, OSEG 11 B, ISEG 59 B (from address 0x69), and a **static** SSEG stack reservation of 92 B (from 0xA4). The .mem report shows the largest contiguous spare internal RAM interval is **1 B** at 0x1F. This is a capacity warning, **not** evidence of a runtime stack collision.

## Compiler-generated storage requests

These totals come from `.ds` directives in the four actual firmware assembly files, and are not physical occupancy numbers.

| Assembly unit | DSEG declarations | ISEG declarations | OSEG declarations |
|---|---:|---:|---:|
| main.asm | 12 B | 59 B | 2 B |
| protocol.asm | 21 B | 0 B | 9 B |
| queue.asm | 24 B | 0 B | 0 B |
| led_matrix.asm | 26 B | 0 B | 3 B |

Do **not** sum these columns to infer free hardware RAM. OSEG is overlaid, the linker contributes library and register-bank storage, and its area extents are not the physical allocation bitmap. The 59-byte indirect queue is already protected by MCU-07B and is not a proposed optimization target.

## Most promising candidate, deliberately not changed

`firmware/main.c` contains `static volatile uint32_t tick_counter` (4 bytes DSEG), incremented by the 8051 T0 ISR every nominal 2 ms. In this version, the 8051 branch does not read this value. The STM32 SysTick handler, by contrast, uses the same value to decide when to scan LEDs.

**Hypothesis:** conditionalize the counter and its increment for STM32 only, leaving 8051 T0 LED scanning otherwise unchanged. This could remove a 4-byte static declaration and some 32-bit ISR work. The actual linked contiguous free space after such a change is **unknown** until a fresh SDCC build and .mem comparison. Interrupt execution timing could change, so it is not a mere documentation fix.

Before approving a production patch, verify all 8051 references and any external observability; build old/new firmware, compare exact linker map, stack interval and HEX; run the queue, protocol, LED and host integration tests; inspect ISR disassembly and timing assumptions. Actual hardware flashing, UART timing, LED and stop behavior remain separate manual verification, not authorized by this diagnosis.

Other apparent storage consumers have greater risk: `protocol.c` 10-byte rx_buf is required by 10-byte frames; `led_matrix.c` 8-byte framebuffer is used by the 8-row display; queue and compiler-generated parameter slots require overlay-aware analysis. No queue-depth or wire-protocol changes are suggested.

## Automatic evidence output

After SDCC compilation, run:

    python3 -m unittest discover -s tests -p 'test_sdcc_ram_profile.py' -v
    python3 scripts/analyze_sdcc_ram.py firmware/build --output firmware/build/ram-source-breakdown.md

The profiler enumerates known .asm modules, rejects malformed .ds entries and missing files, invokes MCU-07B's .map/.mem cross-check, and writes a ranked Markdown report. CI automatically uploads it with other firmware/build artifacts. Reuse the tool on each firmware change, but never interpret static SSEG as measured stack high-water.

Rollback: close this Draft PR, or revert its commits on its isolated branch. The parent MCU-07B PR and main stay intact.
