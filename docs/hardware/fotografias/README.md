# Hardware photographs

Photos that some project fact depends on. Same reason as the datasheets (rule 4
of the [policy](../../POLITICA_DOCUMENTACION.md)): a link to a shop listing
expires, and a photo that only ever existed inside a conversation does not
exist.

**File naming:** `short-description.jpg`, lowercase, no accents.

---

## What belongs here

### Pending: arrived by conversation on 2026-08-03 and could not be archived

| Expected file | What it documents | Status |
|---|---|---|
| `bornera-terminal-adapter.jpg` | `FOR ESP32 TERMINAL ADAPTER` board with both screw columns legible | **Missing** |
| `devkit-modulo.jpg` | The DevKit with the shield silkscreen: `ESP-32`, CE, `FCC ID: 28B77-ESP32-32X` | **Missing** |
| `modulo-reles-songle.jpg` | 4-relay `SRD-05VDC-SL-C` module with optocouplers and JD-VCC jumper | **Missing** |
| `modulo-pc817-2ch.jpg` | 2-channel PC817 optocoupler module, yellow jumpers visible | **Missing** |

### Still to be taken

| Expected file | What it documents |
|---|---|
| `mando-chip-macro.jpg` | Macro of the AiP650EO, marked `19BT450`, with countable pins |
| `mando-serigrafia.jpg` | Board silkscreen: `JK-CH506 Rev1.2`, `G0088-30-4137-202518` |
| `mando-conector-jst.jpg` | The 4-pin JST connector from the back, before soldering |
| `derivaciones-soldadas.jpg` | The three wires soldered with strain relief, before closing up |

---

## Important: the data no longer depends on these photos

Everything the 2026-08-03 photos proved **has been transcribed** into
[HARDWARE.md](../../HARDWARE.md) and marked as verified by photograph:

- The full map of both terminal columns, including the `CLK`, `SD0`, `SD1`,
  `SD2` and `SD3` screws, which are the internal flash and are never touched.
- That **P18 and P4 sit in the same column**, four positions apart, with a GND
  two positions beyond P18.
- The module silkscreen, and the fact that it is **not a WROVER**.

The photos would be needed to *re-check* any of this without the board in hand,
not to know it. Their absence is an annoyance, not a hole.
