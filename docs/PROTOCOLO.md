# The handset bus protocol

> **Status: DECODED.** Verified against the datasheet and against real captures
> from the desk's bus.
>
> Source: *AiP650E Product Specification*, Wuxi I-CORE, doc
> `AiP650E-AX-XS-B037EN`, version 2024-01-B1. Anything marked **[datasheet]**
> comes from there. **[measured]** is our own verification on the board.
> **[captured]** comes from the 2026-08-06 captures in
> [capturas/](capturas/). Anything unmarked is interpretation, and says so.
>
> *Documento en inglés desde el 2026-09-02. La bitácora y los ADR siguen en
> español; ver [POLITICA_DOCUMENTACION.md](POLITICA_DOCUMENTACION.md).*

---

## How the height is read

**[captured]**, verified against what the handset screen shows.

The control box repeats one cycle every **~200 ms**, always the same:

```
S 68 <seg> P     writes digit 1
S 6A <seg> P     writes digit 2
S 6C <seg> P     writes digit 3
S 6E <seg> P     writes digit 4
S 4F <key>  P    reads the keyboard
```

Those four segment bytes, translated through the segment table, **are literally
the number on screen**: the height in whole centimetres, with the fourth digit
blank. `080` = 80 cm, `117` = 117 cm.

**Cross-check of 2026-08-06**, comparing what was read off the bus against what
the person was seeing on the handset at that same moment:

| Action | Screen | Bus |
|---|---|---|
| Manual up | 86 | `086` ✅ |
| Manual down | 77 | `077` ✅ |
| Go to high preset | 117 | `117` ✅ |
| Go to low preset | 80 | `080` ✅ |

**The 117 case is the one that matters**: it confirms the hundreds digit decodes
correctly too. No earlier capture had gone past 99.

### The height refreshes DURING movement ✅

**[captured]**, and this was the risk that could have sunk the whole project.

While the desk moves, the height updates **every centimetre**, without waiting
to arrive. Measured: **~1.2 s per centimetre**, meaning **8.5 mm/s**, identical
going up and down (39 cm in 45.9 s rising, 36 cm in 42.5 s descending).

The first few centimetres after starting take longer, 2.5 to 3 s each, before
settling. That is the motor's acceleration ramp, and it is exactly why
[ADR-001](DECISIONS.md) ruled out counting seconds.

**Consequence: closed-loop control is viable.** The project's real requirement
is confirmed on hardware.

### On pressing a preset, the display announces the destination first

**[captured]**, an unsought finding, and a useful one.

When a preset button is pressed, the screen **blinks the destination height**
before starting to move, and only afterwards counts the real height:

```
102.47   '  7'
102.67   '117'   <- blinks the DESTINATION, three times
103.87   '117'
104.07   '078'   <- and now counts from where it is
104.47   '079'
  ...
150.00   '117'   <- arrives
```

That lets the ESP32 know **where the desk is heading the moment somebody presses
a preset**, not just where it is. For phase 4 it means anticipating the movement
instead of chasing it.

Telling it apart from a real height is straightforward: the destination appears
**before** the height starts changing, and alternates with blank digits.

### Real range and behaviour at the end stops

**[captured]**, from
[2026-08-06-topes-fisicos.log](capturas/2026-08-06-topes-fisicos.log), running
the full range in both directions.

**The desk goes from 73 to 118 cm.** 45 cm of travel.

**Nothing special happens on reaching the stop**, and that is good news:

- **The display keeps showing a number.** No error code appears, and no segment
  pattern the decoder cannot translate. Verified: zero unknown codes in the
  whole capture.
- **It does not blink on hitting the stop.**
- **No new command appears** on the bus.
- The cadence holds at ~1.2 s per centimetre right to the end; the last
  centimetre at the top took 1.4 s. It barely slows.

**Consequence for the firmware:** reaching the stop is indistinguishable from
standing still. The only signal is that **the height stops changing** even
though taps keep being sent. That is the criterion needed to avoid chaining taps
forever against a stop.

### What the display blink means

**[captured]**, corrected against an earlier interpretation.

The display **blinks when continuous travel starts**, not on reaching the stop.
Confirmed across two captures: 0 blinks after a 2.2 s tap that stopped by
itself, 2 and 3 after the presses that triggered continuous travel, and **none
on hitting the stop**.

It gives the firmware a way to detect a movement it did not ask for.

### Display control

**[captured]**, 17 occurrences across all captures.

Command `0x48` is always followed by the **same byte, `0x21`**:

```
S 48 21 P   ->   display=ON, sleep=no, seg=8, brightness=2
```

**The box has never been seen sending the sleep bit.** With the desk idle the
display blanks by writing `0x00` into all four digits, but the chip **does not
sleep** and the bus keeps refreshing every 200 ms. That leaves question 6 open,
about whether it eventually sleeps after much longer, but rules it out over the
minutes the captures cover.

### Command `0x90`: UNIDENTIFIED

**[captured]**, and it is the only part of the protocol still unexplained.

It shows up in **all five captures**, between one and five times each, always in
the same two-byte format and always ACKed:

```
S 90 42 P   ->   unknown
```

It slips in **at the start of a refresh cycle**, in the position where the
`0x68` of the first digit belongs, and that `0x68` arrives ~300 µs later.

It matches no command in the datasheet. It is rare, roughly once every two or
three minutes, and **it does not stop the height being read**, so it blocks
nothing. But it is unexplained, and pretending otherwise would be worse.

### Key codes

**[captured]**, mapped on 2026-08-06 by pressing each button separately.

| Code | Button | How it was identified |
|---|---|---|
| **`0x47`** | **Up** | 15 consecutive reads while rising |
| **`0x57`** | **Down** | 18 consecutive reads while descending |
| **`0x6F`** | **Preset** (the 117 cm one) | One read, right before going to 117 |
| **`0x67`** | **Preset** (the 80 cm one) | One read, right before going to 80 |
| | **Reset** | **Never captured and never will be.** [ADR-008](DECISIONS.md) |

All of them carry **bit 6 set** while the key is pressed, exactly as the
datasheet says. That confirms from real hardware the reasoning in
[ADR-011](DECISIONS.md): simulating a press would require forcing that bit to
one on a bus that can only pull down.

**At rest** the values seen are `0x07`, `0x17`, `0x21`, `0x27` and `0x2F`, with
`0x27` dominant by a wide margin. **The datasheet says no-key should be `0x2E`;
on this board it is not.** A real difference, recorded. What does hold without
exception is that bit 6 is zero in every resting value, and that is the bit that
matters for detecting a press.

### Press timing, measured **[captured]**

| | Duration | How it was measured |
|---|---|---|
| **Recall** a preset (tap) | **< 0.2 s** | Produces a single keyboard read |
| **Store** a preset (hold) | **3.0 s** | 15 consecutive reads before the first blink |

The box **confirms the write with five display blinks every 0.6 s**, which
continue even after the button is released. Detail and capture in
[HARDWARE.md](HARDWARE.md).

Fifteen times the margin between recalling and storing. That is the number
[ADR-010](DECISIONS.md) was missing, and it was blocking the preset wiring.

---

## The chip

**AiP650EO**, marked `19BT450` (a lot code), SOP-16, 1.27 mm pitch.

It is the SOP16 variant of Wuxi I-CORE's **AiP650E**. An LED driver for
**8 segments × 4 digits, common cathode**, with an integrated **7×4** keyboard
scan and support for some key combinations. Software compatible with Titan
Micro's TM1650.

| Parameter | Value | Source |
|---|---|---|
| Supply | 3 to 5.5 V (5 V typical) | [datasheet] |
| Idle current | 0.3 mA typical | [datasheet] |
| Sleep current | 0.05 mA typical | [datasheet] |
| Bus speed | 0 to 4 Mbps | [datasheet] |
| Display refresh cycle | 4 to 20 ms, 8 ms typical | [datasheet] |
| Keyboard scan interval | 20 to 80 ms, 40 ms typical | [datasheet] |

### SOP-16 pinout [datasheet] + [measured]

| Pin | Name | Function | Cable wire |
|---|---|---|---|
| 1 | DIG1 | Digit 1 / keyboard row 1 | |
| **2** | **CLK** | Clock, input. **Internal pull-up** | **Red** |
| **3** | **DIO** | Data, bidirectional. **Open-drain, internal pull-up** | **Green** |
| **4** | **GND** | Ground | **Blue** |
| 5 | DIG2 | Digit 2 / row 2 | |
| 6 | DIG3 | Digit 3 / row 3 | |
| 7 | DIG4 | Digit 4 / row 4 | |
| 8 | A/KI1 | Segment A / column 1. Internal pull-down | |
| 9 | B/KI2 | Segment B / column 2. Internal pull-down | |
| **10** | **VDD** | Supply | **Yellow** |
| 11 | C/KI3 | Segment C / column 3 | |
| 12 | D/KI4 | Segment D / column 4 | |
| 13 | E/KI5 | Segment E / column 5 | |
| 14 | F/KI6 | Segment F / column 6 | |
| 15 | G/KI7 | Segment G / column 7 | |
| 16 | DP/KP | Decimal point | |

**Measured on 2026-08-02:** the four cable wires read **0.2 Ω** against pins 2,
3, 4 and 10. An exact match with the datasheet.

Those 0.2 Ω say something else in passing: **this board does not carry the 220 Ω
series resistors** that the recommended application circuit puts between the
external connector and the CLK/DIO pins. The connection is direct.

### Electrical levels [datasheet]

| Parameter | Value |
|---|---|
| CLK/DIO low level (VIL) | max. 0.2 × VDD = **1.0 V** |
| CLK/DIO high level (VIH) | min. 0.7 × VDD = **3.5 V** |
| CLK internal pull-up (IUP1) | 550 µA typical ≈ **9.1 kΩ** at 5 V |
| DIO internal pull-up (IUP2) | 550 µA typical ≈ **9.1 kΩ** at 5 V |
| Absolute maximum on any input | VDD + 0.5 V |

**This is the figure that unblocked the build.** The bus pull-up does not need
measuring: it is inside the chip and the datasheet specifies it. And if there
were external pull-ups on top, since the recommended circuit carries two of
10 kΩ, the resulting pull-up would only be stronger, never weaker. Sizing the
probe for 9.1 kΩ is safe whatever the case. See [ADR-013](DECISIONS.md).

---

## Who runs the bus

The control box is the **master**. The AiP650E is a **slave**: it decides
nothing, it obeys and answers. All behavioural logic, meaning what counts as a
short pulse, how many milliseconds make a hold, when that means storing a
preset, lives in the control box. The handset is a dumb peripheral.

Two kinds of transaction travel down the cable: display writes, where the box
tells the chip which segments to light, and keyboard reads, where the box asks
which key is pressed.

## Frame format [datasheet]

I2C-like, with START, STOP and ACK, MSB first. **No 7-bit addressing**: the
first byte after START is a fixed command, not an address plus R/W bit. See
[ADR-006](DECISIONS.md).

- START: CLK high and DIO going from high to low.
- STOP: CLK high and DIO going from low to high.
- Data is latched on the **rising edge of CLK**. DIO cannot change while CLK is
  high.
- ACK: on the ninth CLK pulse. In a keyboard read, the command byte's ACK is 0
  and the data byte's ACK is 1.

**A useful rule for the decoder:** **bit 0 of the command byte distinguishes
write from read**. `0x48` (system) and `0x68`, `0x6A`, `0x6C`, `0x6E` (digits)
end in 0 and are writes; `0x49` (read key) ends in 1.

---

## Commands [datasheet]

| Byte | Name | What it does |
|---|---|---|
| `0x48` | System Instruction | Sets system parameters. Followed by the display instruction byte |
| `0x68` | DIG1 address | Followed by the segment byte for digit 1 |
| `0x6A` | DIG2 address | Same for digit 2 |
| `0x6C` | DIG3 address | Same for digit 3 |
| `0x6E` | DIG4 address | Same for digit 4 |
| `0x49` | Get key | Reads the keyboard. The chip answers with one byte |

The `Get key` command is defined as `0100_1XX1`, with bits 2 and 1 don't-care.
So **`0x4B`, `0x4D` and `0x4F` are equally valid**, and the decoder has to mask
those two bits rather than compare against `0x49`.

On power-up, data is transferred to RAM first and the display is turned on
afterwards.

### Display instruction byte [datasheet]

The byte following `0x48`:

| Bit | Name | Meaning |
|---|---|---|
| 6-4 | BR[2:0] | Brightness. `000` = 8 levels, `001` = 1 level … `111` = 7 levels |
| 3 | S | `1` = 7-segment mode, `0` = 8-segment mode |
| **2** | **W** | **`1` = sleep mode on**, `0` = off |
| 0 | D | `1` = display on, `0` = off |

Bits 2 and 0 are the ones that matter for [ADR-012](DECISIONS.md): the control
box **can turn the display off and can put the chip to sleep**. If it does, the
refresh stops and the height being read freezes.

## Segments [datasheet]

| Segment | A | B | C | D | E | F | G | DP |
|---|---|---|---|---|---|---|---|---|
| Bit | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |

The digit table, which is all that is needed to read the height:

| Digit | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
|---|---|---|---|---|---|---|---|---|---|---|
| Byte | `0x3F` | `0x06` | `0x5B` | `0x4F` | `0x66` | `0x6D` | `0x7D` | `0x07` | `0x7F` | `0x6F` |

With bit 7 set, the digit carries a decimal point: `74.5` on screen is the bytes
`0x07`, `0xE6`, `0x6D`.

**The height decoder can be written before capturing anything.** The capture
confirms; it does not discover.

## Keyboard [datasheet]

On a `Get key` command the chip answers with one byte. Official table:

| | DIG1 | DIG2 | DIG3 | DIG4 |
|---|---|---|---|---|
| **NO KEY** | `00_101_110` = **`0x2E`** | ← the same for all four | | |
| KI1 | `01_000_100` `0x44` | `01_000_101` `0x45` | `01_000_110` `0x46` | `01_000_111` `0x47` |
| KI2 | `0x4C` | `0x4D` | `0x4E` | `0x4F` |
| KI3 | `0x54` | `0x55` | `0x56` | `0x57` |
| KI4 | `0x5C` | `0x5D` | `0x5E` | `0x5F` |
| KI5 | `0x64` | `0x65` | `0x66` | `0x67` |
| KI6 | `0x6C` | `0x6D` | `0x6E` | `0x6F` |
| KI7 | `0x74` | `0x75` | `0x76` | `0x77` |
| KI1+KI2 | `0x7C` | `0x7D` | `0x7E` | `0x7F` |

Structure: **bit 6 = key pressed**, bits 5-3 = KI column, bit 2 = always 1, bits
1-0 = DIG row.

Behavioural rules [datasheet]:

- **A press is only recognised if it lasts at least two scan periods.** With the
  interval between 20 and 80 ms, the real floor is **160 ms** in the worst case.
  This sets the minimum relay pulse duration, see [HARDWARE.md](HARDWARE.md).
- KI1+KI2 combinations on the same DIG have **top priority**.
- If several keys are down at once, **the lowest code wins**.

### Why this closes the door on injection [datasheet]

With no key, the chip returns `0x2E` = `0010_1110`. With a key, for example
`0x44` = `0100_0100`.

Manufacturing a press would mean driving **bit 6 from 0 to 1**. On an open-drain
bus a bit can only be forced to **0**; the 1 is set by the pull-up and by
nothing else. It is not difficult, it is impossible. See
[ADR-011](DECISIONS.md).

**The only thing that can be done, and it is no use:** by forcing bits to 0 you
can mask a real press, or turn one key into another with a lower code, since
`0x74` (KI7) can be degraded to `0x44` (KI1). Both require somebody physically
pressing, so neither helps with automation.

**Forbidden:** driving DIO high with a push-pull output. While the AiP650E is
pulling that line down, that is a short circuit against the chip's output.

---

## Keyboard matrix and handset build [datasheet]

The buttons close a **KI** column against a **DIG** row. The KI lines have
internal pull-downs (30 to 90 µA) and the DIG lines are outputs. The application
circuit recommends **2 kΩ in series on each DIG line** inside the key matrix, so
presses do not disturb the display.

That explains the **5 V measured across a button's pins**: the DIG line is high
and the KI is low through its pull-down. See [ADR-004](DECISIONS.md).

For relay actuation, the relay contact goes **in parallel with the button**,
doing exactly what the button does.

---

## Open questions

### Answered by the 2026-08-06 captures ✅

1. ~~**Which digits does it use, and in what order?**~~ → `68`, `6A`, `6C`, `6E`
   in that order, left to right. The fourth is always blank. No decimal point:
   the height is whole centimetres.
2. ~~**How fast does the bus run?**~~ → **~202 kHz**, and that figure is a
   floor. It forced the capture to be rebuilt: interrupts could not keep up, and
   it moved to burst sampling.
3. ~~**How often does the height refresh?**~~ → full cycle every **~200 ms**;
   the height changes every **~1.2 s**, which is how long 1 cm of movement
   takes.
4. ~~**Which key code is each button?**~~ → up `0x47`, down `0x57`, presets
   `0x6F` and `0x67`. Table above.
5. ~~**Does the height refresh during movement?**~~ → **Yes.** Every centimetre,
   without waiting to arrive. **Closed-loop control is viable.**

### Still open

6. ~~**Does it turn the display off or put the chip to sleep on inactivity?**~~
   → **It blanks the display but does NOT sleep the chip.** Measured with the
   desk idle for **15 minutes**: the sniffer armed **4505 times and not once
   found the bus silent**. Within seconds the screen blanks by writing `0x00`
   into all four digits, but the 200 ms refresh never stops, and the control
   command **always says `sleep=no`**.
   **Consequence:** the ESP32 can tell *"there is no height to show"* from
   *"there is no bus"*, which is exactly what [ADR-012](DECISIONS.md) needed.
   *Unverified: what happens after hours, or on returning from a power cut.*
9. **What is the `0x90 42` command?** It appears in all five captures, one to
   five times each, and it is not in the datasheet. See above. It blocks
   nothing.
7. **Does any function use a key combination?** The chip supports it. If storing
   a preset or the reset were combinations, a single relay would not reproduce
   them. Nothing in the captures suggests it yet, but it has not been tested on
   purpose.
8. ~~**How long must M1 or M2 be held before it stores?**~~ → **3.0 s**, measured
   on 2026-08-06. See above. Answered.

---

## Our own confirmation table

What was observed on the real bus, which at one point **differs from the
datasheet**.

| Byte observed | Interpretation | Capture |
|---|---|---|
| `68` `6A` `6C` `6E` | Addresses of the four digits, left to right | both from 2026-08-06 |
| `4F` | Keyboard read command. Matches the `0100_1XX1` mask | both |
| `0x27` | Keyboard at rest, dominant value. **The datasheet says `0x2E`** | both |
| `0x07` `0x17` `0x21` `0x2F` | Other resting values, all with bit 6 clear | both |
| `0x47` | **Up** key pressed | [pulsadores](capturas/2026-08-06-pulsadores.log) |
| `0x57` | **Down** key pressed | pulsadores |
| `0x6F` | **Preset** key (117 cm) | pulsadores |
| `0x67` | **Preset** key (80 cm) | pulsadores |
| `0x00` in all 4 digits | Display blanked, bus still refreshing | both |

<details>
<summary>Original table, before the captures</summary>

| Byte observed | Interpretation | Capture |
|---|---|---|
| | | |

*It stayed empty and was filled in one go by the two captures of 2026-08-06.*

</details>

---

> **Corrections on record.** An earlier version of this file gave the pinout as
> 5 = SCL, 6 = SDA, 15 = GND, 16 = VDD, taken from components101. **It was
> wrong**, and the measurement disproved it. Another version warned that the
> commands and keyboard format came from TM1650 sources rather than the real
> chip; that warning no longer applies, since the I-CORE datasheet confirms them
> one by one. Detail in the [log](BITACORA.md).
