# Final board: manufacturing specification

> Replaces the breadboard build, which was **the fragile link in the whole
> project**: a short between two bus wires and a badly seated optocoupler cost
> two days of diagnosis and nearly got the handset written off as dead (log
> entry of 2026-08-23).
>
> Status: **designed, not manufactured.** Decisions taken with the owner on
> 2026-08-24.

---

## What this board does

It connects an **ESP32 DevKit** to the bus and to the buttons of a **Jiecang
JK-CH506** standing desk handset, so the height can be read and the buttons
driven.

- **Reads** the handset's two-wire bus (CLK and DIO), never writing to it
- **Drives** the four buttons through isolated dry contacts
- **Isolates** the bus from the ESP32 with a buffer powered by the handset itself

---

## Design change: the isolation buffer

**This is the main reason to rebuild the board**, not the looks.

### The problem it solves

On the breadboard the bus reaches the GPIO **directly** through a divider. When
the ESP32 loses power while the desk is on, whether from swapping its supply, a
power cut or a loose cable, the **GPIO protection diode conducts** and drags the
bus line down. The handset stops working.

It happened **several times**, and its resemblance to a real fault cost two days
([ADR-019](../DECISIONS.md), [ADR-031](../DECISIONS.md)).

### The solution

A **74HC14** (six Schmitt trigger inverters) **powered from the handset's own
5 V**, inserted between the bus and the ESP32:

```
                    ┌─────── 74HC14 ────────┐
BUS CLK ────────────┤ inv1 ──> inv2 ────────┼──[10k]──┬──> GPIO18 (ESP32)
                    │                       │         └──[18k]── GND
BUS DIO ────────────┤ inv3 ──> inv4 ────────┼──[10k]──┬──> GPIO4  (ESP32)
                    │                       │         └──[18k]── GND
                    └───────────────────────┘
                      VCC = 5 V from the handset
```

**Two inverters per line, not one.** The 74HC14 inverts, and two inversions in a
row give the original signal back. The Schmitt trigger also **cleans up the
edges**, which on a 202 kHz bus read by sampling is far from wasted.

**With this, if the ESP32 dies the bus never notices.** The buffer stays powered
by the handset and its input is high impedance. The output divider no longer
hangs off the bus but off the buffer output, which is low impedance.

⚠️ **This uses the yellow wire (5 V), which until now was never touched.**
[ADR-007](../DECISIONS.md) forbade it for a specific reason: powering the
**ESP32** from it, since the ESP32 spikes into the hundreds of milliamps. **A
74HC14 draws microamps**, less than the handset's own chip. The prohibition
still stands for the ESP32; this exception is for the buffer and for nothing
else.

---

**Schematics drawn across three sheets**, sent to the manufacturer along with
this document:

| Sheet | What it shows |
|---|---|
| [1 · System](plano_pcb_1_sistema.svg) | How the board is inserted between the box and the handset |
| [2 · Bus](plano_pcb_2_bus.svg) | The 4 wires passing through and the tap to the buffer |
| [3 · Channels](plano_pcb_3_canales.svg) | The four optocouplers and their wiring |

*(A first attempt put all three on a single sheet and came out illegible. A
schematic you have to decipher is no use for manufacturing.)*

## Electrical schematic

### Section A: bus reading (isolated)

| Component | Value | Connection |
|---|---|---|
| U2 | 74HC14 (DIP-14) | VCC = JST1.4 (handset 5 V) · GND = common |
| C1 | 100 nF ceramic | Between VCC and GND of U2, **as close to the chip as possible** |
| R1 | 10 kΩ | inv2 output (U2 pin 4) → GPIO18 |
| R2 | 18 kΩ | GPIO18 → GND |
| R3 | 10 kΩ | inv4 output (U2 pin 8) → GPIO4 |
| R4 | 18 kΩ | GPIO4 → GND |

**Resulting level at the GPIO:** 5 V × 18/(10+18) = **3.21 V**. Inside the
absolute maximum of 3.6 V, with margin.

**Inverter chaining on the 74HC14:**

| Line | Input | Intermediate output | 2nd inv input | Final output |
|---|---|---|---|---|
| CLK | pin 1 (1A) | pin 2 (1Y) | pin 3 (2A) | pin 4 (2Y) |
| DIO | pin 5 (3A) | pin 6 (3Y) | pin 9 (4A) | pin 8 (4Y) |

Inverters 5 and 6 (pins 11, 12 and 13) are unused, so **their inputs are tied to
GND**, never left floating. A floating CMOS input oscillates and draws current.

### Section B: actuation (four channels)

Identical for all four. With `n` = 1..4:

| Component | Value | Connection |
|---|---|---|
| Rn (R5-R8) | 330 Ω | GPIO → LED anode (PC817 pin 1) |
| Un (U3-U6) | PC817 (DIP-4) | pin 2 = GND · pins 3 and 4 = to the button |

| Channel | GPIO | Button | Connector |
|---|---|---|---|
| 1 | 27 | Up | JST2 pins 1-2 |
| 2 | 26 | Down | JST2 pins 3-4 |
| 3 | 25 | Preset 1 | JST3 pins 1-2 |
| 4 | 33 | Preset 2 | JST3 pins 3-4 |

**330 Ω gives 6.2 mA** through the LED with the GPIO at 3.3 V and a forward drop
of 1.25 V. Well under what an ESP32 GPIO supplies comfortably.

⚠️ **Pins 3 and 4 of the PC817 are the handset side and have no critical
polarity in the layout**, but **they must never be joined to pins 1 and 2**.
That would tie the handset's 5 V to the GPIO and destroy the isolation, which is
the whole reason the optocoupler is there.

### Section C: ESP32

**38-pin DevKit mounted on sockets** (two 19-way female strips, 2.54 mm pitch,
25.4 mm between centres).

Pins used: **18, 4** (bus, input) · **27, 26, 25, 33** (channels, output) ·
**3V3, GND**.

⚠️ **GPIO 0, 2, 5, 12 and 15 are not used**: they are strapping pins and their
level at boot decides how the chip starts. **GPIO 6-11 are not used either**:
they go to the flash memory. See [ADR-020](../DECISIONS.md).

---

## Connectors

**The board is inserted into the handset cable**, decided with the owner on
2026-08-24:

```
[control box] ──4 wires──> [BOARD] ──4 wires──> [handset]
                              ^                     │
                              └───8 wires───────────┘
                                   (buttons)
```

**Why this way rather than as a tap:** it removes **the three bus solder joints
inside the handset**, which are exactly the ones that produced the green-yellow
short of 2026-08-23 and two days of diagnosis. Inside the handset only the eight
button wires remain, and there is no way around those: the contacts are in
there.

⚠️ **The trade-off, and it needs keeping in mind: the board is now IN SERIES.**
As a tap, a failed board still left the handset working. Inserted in the cable,
**a broken trace or a loose connector leaves the desk with no handset.** Mitigate
it:

- Route the four pass-through wires as **direct, wide traces** (≥ 0.5 mm), not
  going through any component
- Keep a **jumper cable** on hand that ties `BUS-IN` straight to `BUS-OUT`, to
  bypass the board if it ever fails

**Five connectors, all 4-way, 2.54 mm pitch:**

| Ref | Name | 1 | 2 | 3 | 4 |
|---|---|---|---|---|---|
| **J1** | `BUS-IN` (from the box) | CLK (red) | DIO (green) | GND (blue) | +5V (yellow) |
| **J2** | `BUS-OUT` (to the handset) | CLK | DIO | GND | +5V |
| **J3** | `UP/DOWN` | Up a | Up b | Down a | Down b |
| **J4** | `PRESET 1/2` | P1 a | P1 b | P2 a | P2 b |

**J1 and J2 are joined pin to pin by pass-through traces.** The taps to the
buffer (CLK, DIO) and the supply for U2 (+5V, GND) branch off those traces.
**The bus is neither interrupted nor switched: it is only observed.**

⚠️ **The connector type for J1 and J2 depends on what the original cable uses.**
The factory cable has a connector at both ends, so **nothing needs cutting**:
either make two short patch leads, or fit the same connector type on the board.
**Still to be identified**, see the note at the end.

**Button pairs have no polarity**: they are the two ends of a dry contact. The
order inside each pair does not matter.

⚠️ **JST1 pin 4 carries real 5 V from the handset.** It must not reach any ESP32
pin; it only powers U2. Route it away from everything else.

---

## Bill of materials

| Ref | Component | Qty | Note |
|---|---|---|---|
| U1 | Socket for 38-pin ESP32 DevKit | 2 strips of 19 | Female, 2.54 mm |
| U2 | 74HC14N (DIP-14) | 1 | With socket |
| U3-U6 | PC817 (DIP-4) | 4 | **With sockets**: a loose one already caused a fault |
| R1, R3 | 10 kΩ ¼ W | 2 | Divider, upper leg |
| R2, R4 | 18 kΩ ¼ W | 2 | Divider, lower leg |
| R5-R8 | 330 Ω ¼ W | 4 | Optocoupler LEDs |
| C1 | 100 nF ceramic | 1 | Decoupling for U2 |
| J1, J2 | 4-way connector for the bus | 2 | **Type pending**: must mate with the original cable |
| J3, J4 | JST-XH 4-way, straight male | 2 | Plus female housings and crimp contacts |

---

## Layout notes

1. **Common, solid ground.** ESP32, 74HC14, PC817 and the bus GND share ground.
   Continuous ground plane if the board is double sided.
2. **C1 right next to U2.** A decoupling capacitor far from the chip is useless.
3. **Keep the handset side apart.** Traces from JST2 and JST3 to pins 3 and 4 of
   the optocouplers **must not come near** the ESP32 traces: they are the two
   sides of a galvanic isolation. **Leave at least 2 mm** between them, and run
   no ground plane underneath.
4. **The +5 V from JST1 reaches U2 only.** None of its traces should approach
   the ESP32 pins.
5. **Silkscreen:** name of every connector, channel number beside each
   optocoupler, and pin 1 marked on every integrated circuit.
6. **Mounting holes** in all four corners, 3 mm.

---

## What carries over from the current build

The divider values **change**, from 16.3k/27k to 10k/18k, because they now hang
off the buffer output rather than off the bus. Loading the bus is no longer a
concern, and a lower impedance gives cleaner edges.

Everything else, meaning channels, LED resistors and pin assignment, carries
over untouched and **verified in operation over days**.

---

## Before ordering it

⚠️ **The buffer has never been tested.** Everything else on this board has been
running for days, but section A is new design. **Build it on breadboard first**
and check two things before spending money on a PCB:

1. That the sniffer still reads the bus at the same quality, comparable against
   the measured reference of **0.67% malformed transactions**
2. That **with the ESP32 disconnected, the handset keeps working**, which is
   literally what it is being added for

### Pending data: the original cable connector

To specify J1 and J2, the connector on the 4-wire cable running from the box to
the handset has to be identified. What to look at:

- **Pin spacing** (pitch): 2.0 mm and 2.54 mm are the usual ones
- **Body shape**: whether it has a retention latch and on which side
- **Marking on the plastic**, if any: `XH`, `PH`, `ZH`, `SM` and so on

That is enough to order the right part. If it turns out to be a 2.54 mm JST-XH,
which is the most likely on handsets of this kind, **all four board connectors
would be the same type**, and a 4-wire cable with female connectors at both ends
covers the board to handset run.
