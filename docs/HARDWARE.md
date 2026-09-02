# Hardware

> Verified physical facts. Every claim states whether it is **verified**
> (measured or observed) or an **assumption**.
>
> *Documento en inglés desde el 2026-09-02. La bitácora y los ADR siguen en
> español; ver [POLITICA_DOCUMENTACION.md](POLITICA_DOCUMENTACION.md).*

---

## Inventory

- **38-pin ESP32 DevKit** on an expansion board with screw terminals.
  Silkscreen: `HW-395 V0.0.3` / `DT-Y6593`.
- **4-relay module**, Songle SRD-05VDC-SL-C, with optocouplers and a JD-VCC
  jumper.
- Assorted ¼ W resistors, see the table below.
- Multimeter. Soldering iron and solder.
- **There is no** oscilloscope and no logic analyser. This shapes everything:
  the ESP32 itself has to serve as the measuring instrument.
- MacBook Air M4 for programming over USB.

---

## The desk and its handset

A motorised-column standing desk, sold under the **Cougar** brand. Cougar makes
peripherals, not actuators: the mechanism is white-labelled, and the `JK-CH506`
silkscreen on the handset points to **Jiecang**. Searching for documentation
under "Cougar" leads nowhere; under Jiecang or TM1650, it does.

**Two motorised columns**, joined to each other by a **6-wire** cable. The brain
sits in the control box. **It has not been opened and does not need opening.**

Power: an external **29 V** adapter *(verified, read off the adapter label)*.
The control box steps those 29 V down to the 5 V that powers the handset.

That 6-wire cable between motors most likely carries **the 29 V** and the Hall
sensors. **It is not touched**: the Hall sensors would give position by counting
pulses, which is exactly what [ADR-001](DECISIONS.md) ruled out, and doing so
would mean tapping wiring carrying motor current at 29 V. The display already
gives absolute height, in centimetres.

### External connectors on the control box, VERIFIED

*Checked by visual inspection on 2026-08-03, without disassembling anything.*

| Connector | What it is |
|---|---|
| Adapter input | 29 V |
| Handset connector | 4 wires |
| Motor cable | 6 wires |

### The bus pull-up lives in the control box, VERIFIED 2026-08-23

Measured **with the handset disconnected**, on the cable coming out of the box:

| Between | Resistance |
|---|---|
| Red (CLK) ↔ yellow (5 V) | **21 kΩ** |
| Green (DIO) ↔ yellow (5 V) | **22 kΩ** |

**Both lines have a pull-up in the box.** That fits the 2.4 kΩ measured on
2026-08-06 *with the handset connected*: those were these ~21 kΩ in parallel
with a stronger one inside the handset.

**Consequence:** a handset replacement **only needs to pull the lines low**; the
high level is set by the box. That was the condition needed for the ESP32 to be
able to take the handset's place. See [ADR-032](DECISIONS.md).

**Reference read off the motor on 2026-08-23: `FELV3-F4.0`.** Searched for with
no result: it appears in no public catalogue and at no distributor. **It is a
manufacturing code, like `JK-CH506`**, the same pattern. It is no use for
finding spares by part number.

**There is no other connector.** In particular, **there is no accessory port**,
RJ11, RJ12 or RJ45. That rules out the shortcut of path C in
[PLAN.md](PLAN.md), those Jiecang boxes with a 9600 8N1 serial port and
ready-made ESPHome components, and confirms that **the handset bus is the only
way in**. It was to be expected: a handset with an AiP650E indicates a
budget-range box.

Having two motors has another consequence:

- The two columns **can fall out of sync** after a power cut or an overload,
  leaving the desk crooked. Recalibrating is what the reset button is for. That
  reinforces [ADR-008](DECISIONS.md): the button stays out of the circuit and
  recalibration is always manual, looking under the desk.

### Handset

- PCB marked `JK-CH506 Rev1.2` and `G0088-30-4137-202518` (a second reading off
  a photograph; the original handover transcribed `G9008B-3E-4137-202518`). The
  `202518` suffix fits a date code: **week 18 of 2025**. "JK" points to
  **Jiecang**, a Chinese manufacturer of desk actuators.
- `JK-CH506` **does not appear in Jiecang's public catalogue**, whose handsets
  are called `JCHT35Kxx`. It is a manufacturing or customer reference, not a
  catalogue one: there is no official documentation to find. The useful
  documentation is the chip's.
- 5 tactile buttons and a 7-segment LED matrix display showing the desk's
  current height.
- A single chip: **AiP650EO**, marked `19BT450`, SOP-16. `19BT450` is a lot
  code, not a variant. A functional clone of Titan Micro's **TM1650**; the clone
  maker is I-CORE (Wuxi), which is the vertical-bars logo on the package. An LED
  display driver for **8 segments × 4 digits, common cathode**, with an
  integrated 7×4 keyboard scanner and support for some combinations. Two-wire
  I2C-like interface. Supply 3 to 5.5 V. Full pinout and protocol in
  [PROTOCOLO.md](PROTOCOLO.md).
- **There is no microcontroller in the handset.** *Verified by inspection: the
  AiP650EO is the only chip on the board.* This is what makes the whole project
  viable, because it means the control box sends the literal on-screen digits
  down the cable.
- It connects to the control box with a **4-wire** cable, a white 4-pin JST
  connector, small pitch (PH or ZH type).

---

## Cable pinout, VERIFIED

*Verified on 2026-08-02 by continuity, with the connector unplugged from the
control box and the multimeter in ohms. All four readings gave 0.2 Ω, which is
trace resistance: these are direct connections, not indirect paths.*

| Colour | AiP650 pin | Function | Status |
|---|---|---|---|
| **Red** | 2 | **CLK** (SCL), bus clock | **Verified** |
| **Green** | 3 | **DIO** (SDA), bus data | **Verified** |
| **Blue** | 4 | **GND**, ground | **Verified** |
| **Yellow** | 10 | **VDD**, 5 V | **Verified** |

All four wires land exactly on the four functions a handset cable has to carry,
and they match the pinout in the I-CORE datasheet. The manufacturer calls the
lines **CLK** and **DIO**, not SCL/SDA.

### Correction: the handover had red and yellow wrong

The [original handover](historia/HANDOVER-2026-07-27.md) gave **red** as "VCC
(5 V) probable" and yellow and green as the data lines. Two of the four are the
other way round:

| Wire | Handover said | It actually is |
|---|---|---|
| Red | VCC (assumed) | **SCL** |
| Yellow | Data | **VDD, 5 V** |
| Green | Data | SDA, correct |
| Blue | GND | GND, correct |

Consequence had the old plan been built: the **clock would have been left
unconnected**, so the sniffer would have captured nothing, and the **5 V line
would have gone to a GPIO** through a divider. Two faults at once, and the
symptom (no traffic visible) pointed at neither of them.

The error came from a multimeter being unable to tell an idle-high data line
from a supply line: all three non-blue wires read ~4.x V alike. That is why the
handover marked it as *assumed*, and why it was never built.

**Therefore:** the dividers go on **red** and **green**. The wire that is **not
connected** is the **yellow** one. Blue still goes to GND.

Note: none of the 4 wires shows continuity to the motor's metal chassis. Normal,
whether from a floating supply or a painted chassis.

---

## The buttons

*Function verified by using the handset.*

| Button | Function | Automatable? |
|---|---|---|
| Up | Rises while held | Yes |
| Down | Descends while held | Yes |
| M1 | Goes to stored height 1 | Yes, the preferred route |
| M2 | Goes to stored height 2 | Yes, the preferred route |
| Reset | Recalibration | **No. Never.** See [ADR-008](DECISIONS.md) |

### Button behaviour

*Verified by using the handset. **The timings are now measured**, see
[Relay actuation timing](#relay-actuation-timing) below: recalling a preset
takes < 0.2 s and storing one requires 3.0 s.*

| Action | Result |
|---|---|
| Short press on up/down | Moves ≈ 1 cm and stops |
| Hold up/down | Moves to the physical limit |
| Any button during movement | **Stops the movement** |
| Short press on M1/M2 | Goes to the stored height |
| **Hold M1/M2** | **Stores the current height in that preset** |

Two consequences that govern the actuation design:

**The same button that recalls a preset overwrites it if held.** A relay that
closes for too long does not go to the stored position: it destroys it. The time
threshold separating "go to" from "store" **has to be measured** and pulses must
stay well below it. See [ADR-010](DECISIONS.md).

**"Any button stops it" is a brake.** Any available relay serves to abort a
movement in progress.

Physical detail:

- 4-pin tactile buttons. The pins on the same side are internally joined; the
  **diagonal** pair is what the button closes when pressed.
- **Verified: there are 5 V across a button's pins.**
- Consequence: they cannot be connected to an ESP32 GPIO, which tolerates 3.6 V.
  See [ADR-004](DECISIONS.md).

---

## Tap point

On the handset board, the 4 pins of the JST connector are soldered and reachable
from the back. Thin wires (AWG 28 to 30) are soldered on **leaving the original
connector in place**, so the handset keeps working throughout.

**Only three wires. The yellow one is not soldered.**

| Connector pin | Function | Where it goes |
|---|---|---|
| Where **red** enters | CLK | Divider → P18 |
| Where **green** enters | DIO | Divider → P4 |
| Where **blue** enters | GND | ESP32 GND |
| Where **yellow** enters | VDD 5 V | **Nowhere. Not soldered.** |

Leaving yellow unsoldered is not just one joint saved: it avoids having a loose
5 V wire near a breadboard full of GPIOs that tolerate 3.6 V.

### Procedure

**Preparation.** Desk unplugged from mains. Board out of its case and secured to
the table. Three wires of 20 to 30 cm in different colours, **noting which
colour goes to which pin**, because once on the breadboard they are
indistinguishable.

The connector joints already carry solder, so it is enough to reflow them and
rest the tinned wire on top. Little iron time per joint: pressing on with the
tip is how a trace gets lifted.

### Strain relief, not optional

The failure that ruins the handset is not the solder, it is the tug: a thin wire
soldered to a trace, if somebody pulls it, **rips the trace off**. Nothing that
comes afterwards matters if that happens.

As soon as all three wires are on, **tape them to the board a couple of
centimetres from the joints**, so any pull is taken by the tape and not by the
copper.

### Check before closing up

This takes advantage of the chip map already being verified:

| New wire | Must show continuity to pin |
|---|---|
| The one on the red pin | 2 (CLK) |
| The one on the green pin | 3 (DIO) |
| The one on the blue pin | 4 (GND) |

No continuity means a cold joint. And also check there is **no** continuity
between neighbouring wires: a solder bridge between two connector pins is easy
to make and invisible to the eye.

**Afterwards, reassemble the case and use the handset normally.** Up, down, look
at the display. If it works as before, the taps are fine. If anything changed,
it gets resolved **before** connecting the ESP32, because afterwards there would
be no telling who to blame.

### If a trace lifts

Not the end. The connector trace runs to the chip pin, so the wire can be
soldered directly to the corresponding AiP650E pin. It is harder, at 1.27 mm
pitch, but it is the same signal. Before trying, check with the multimeter where
whatever remains of the trace actually goes.

---

## Available resistors

**All measured with a multimeter on 2026-08-02.** This entirely replaces the
previous inventory, which was read off colour codes and had at least three wrong
values. 30 pieces.

| Measured value | Quantity |
|---|---|
| 2 Ω | 2 |
| 4.5 Ω | 3 |
| 5.4 Ω | 2 |
| 6.7 Ω | 2 |
| 36 Ω | 2 |
| 54 Ω | 2 |
| 75 Ω | 2 |
| 81 Ω | 2 |
| 555 Ω | 1 |
| 800 Ω | 1 |
| **1.8 kΩ** | 3 |
| **3.3 kΩ** | 1 |
| **7.4 kΩ** | 2 |
| **9.1 kΩ** | 2 |
| 74 kΩ | 1 |
| 240 kΩ | 1 |
| 2.2 MΩ | 1 |

**Bought and received on 2026-08-03**, measured with a multimeter before use as
[ADR-015](DECISIONS.md) requires:

| Measured value | What for |
|---|---|
| **26.79 to 27.01 kΩ** (batch) | Lower leg of the probe divider ([ADR-016](DECISIONS.md)), **unblocks phase 2** |
| 10 kΩ | Pull-downs for the control GPIOs, phase 3 |
| 330 Ω | Optocoupler input LEDs, phase 3 |

**The 27 kΩ ones, measured piece by piece, fall between 26.79 and 27.01 kΩ.**
With the 9.1 kΩ from the drawer on top, the divider comes out as:

| With lower R of | Bus high level | Voltage at the GPIO |
|---|---|---|
| 26.79 kΩ | 3.99 V | 2.98 V |
| 27.01 kΩ | 3.99 V | 2.99 V |

Exactly on the ADR-016 design. The extremes of the batch differ by 10 mV at the
GPIO, so **it makes no difference which two pieces are used**, and since they
differ by 0.8% between themselves, both channels behave alike. Rising-edge
impedance: 10.8 kΩ.

It also confirms the ratio expected by the [ADR-018](DECISIONS.md) check:
3.99 / 5 = **0.80**.

### Where the confusion came from

The two pieces read as brown-green-orange (15 kΩ) and then as
violet-green-orange (75 kΩ) are actually **violet-green-red, 7.4 kΩ**. There is
only one 74 kΩ, the one already counted.

Three colour readings turned out wrong before measuring, and two of them changed
the probe design, one unblocking it and one voiding it. The cause is not
carelessness: the colour code **has no redundancy**, and one misread colour
moves the value by a factor of a thousand. Hence the rule in
[ADR-015](DECISIONS.md): values get measured, not read.

### Why there are not enough, and by how much

Each divider needs to add up to about **26 kΩ** for the bus high level to stay
above 3.7 V with the internal 9.1 kΩ pull-up. Two dividers: 52 kΩ.

All the stock between 1 kΩ and 10 kΩ adds up to **41.7 kΩ**: 3 × 1.8 k, 1 × 3.3
k, 2 × 7.4 k and 2 × 9.1 k. About ten short.

*Precision on record: this used to say 43 kΩ. That figure included the 800 Ω and
555 Ω pieces, which are below 1 kΩ and are no use here. The conclusion, that
there are not enough, does not change.*

The 74 kΩ does not rescue the situation: it allows one correct line to be built,
74 kΩ below with 36 kΩ above made of 9.1 + 9.1 + 7.4 + 7.4 + 3.3, but it
consumes all the mid-range stock, and the second line is left with 7 kΩ, which
puts the bus at 2.2 V. Useless.

### What to buy

⚠️ **Corrected on 2026-08-22: the diagram below said 9.1 kΩ on top, and what is
built is 16.3 kΩ** ([ADR-022](DECISIONS.md), which superseded ADR-016 on
2026-08-06). The text was not updated then, and the error propagated into the
headers of the captures written that day. **Measured on the real build: 16.5 kΩ
between the bus wire and the GPIO, and 43.5 kΩ from bus to ground**, which is
16.3 + 27 in series. The good value is ADR-022's.

**Two resistors and it can be built.** The measured ones go on top of both
lines; only the lower ones are missing.

| Purchase | Bus high level | Voltage at the GPIO |
|---|---|---|
| **2 × 27 kΩ** ← preferred | 3.99 V | 2.99 V |
| 2 × 22 kΩ | 3.87 V | 2.74 V |
| 2 × 33 kΩ | 4.12 V | 3.23 V |

All three meet the requirement (bus ≥ 3.5 V, GPIO between 2.5 and 3.6 V). The
27 kΩ leaves both voltages better centred.

A **labelled assortment** is still worth it (600 to 1000 pieces, 5 to 8 €):
phases 3 and 4 will ask for more resistors.

---

## Probe circuit

### Valid design, [ADR-016](DECISIONS.md)

Pending two 27 kΩ resistors. Everything else is in hand.

**Schematic: [hardware/plano_sonda_v2.svg](hardware/plano_sonda_v2.svg)**, the
only one that gets built.

```
RED   (CLK) ──[16.3k]─┬──> ESP32 P18
                      └──[27k]──> GND

GREEN (DIO) ──[16.3k]─┬──> ESP32 P4
                      └──[27k]──> GND

BLUE  (GND) ──────────────> ESP32 GND

YELLOW (5 V) ─────────────  NOT CONNECTED
```

| | Value | Required |
|---|---|---|
| Load on the bus | 36.1 kΩ | |
| Bus high level | 3.99 V | ≥ 3.5 V |
| Voltage at the GPIO | 2.99 V | ≥ 2.5 V and ≤ 3.6 V |
| Impedance seen by the GPIO, **falling** edge | 6.8 kΩ | |
| Impedance seen by the GPIO, **rising** edge | **10.9 kΩ** | |

The GPIO goes **to the node between the two resistors**, never to an end.

**Correction on record.** ADR-016 and this file gave 6.8 kΩ as the impedance,
full stop. That value only holds while the chip pulls the line low; on the
rising edge the source is the internal pull-up, which sits in series with the
upper resistor, and the real impedance is (9.1 + 9.1) ∥ 27 = **10.9 kΩ**. The
slow edge on an open-drain bus is always the rising one, so that is the number
that governs. It changes neither the design nor the order of preference between
probe versions. Detail in [ADR-018](DECISIONS.md).

### Check on connecting it: by ratio, not absolute value

**A multimeter averages, and the bus is switching.** Both readings will come out
below the calculated value, by an amount that depends on the bus duty cycle and
is not known until the first capture. So the criterion is not an absolute
threshold but the relationship between the two measurements
([ADR-018](DECISIONS.md)):

1. **With the probe disconnected**, desk powered and still, display showing a
   stable number: measure red to blue and green to blue. Write it down.
2. **With the probe connected**, without touching any button and with the
   display showing the same number: measure the same two points. Write it down.
3. Divide the second reading by the first, on each line.

| Ratio | Real bus high level | What to do |
|---|---|---|
| **≥ 0.75** | ≥ 3.75 V | Correct, carry on. 0.80 is what to expect |
| 0.70 to 0.75 | 3.50 to 3.75 V | Works, thin margin. Stop and think |
| **< 0.70** | **< 3.50 V** | **Disconnect.** Below the chip's VIH |

If the desk moved or something was pressed between the two measurements, the
duty cycle changed and the ratio means nothing: repeat it.

As a bonus, the ratio gives the real internal pull-up, which so far was only
known from the datasheet as a typical value:
`Rpull-up = 36.1 kΩ · (1 - ratio) / ratio`.

**The ESP32 has to be USB powered before the wires reach the divider**
([ADR-019](DECISIONS.md)). With the ESP32 unpowered and the probe attached, the
bus sags to about 2.85 V and the handset fails. No damage, but with a symptom
that points at the solder joints.

Earlier designs, kept as history: [ADR-013](DECISIONS.md) (15 k / 33 k, valid
but with no parts), [ADR-014](DECISIONS.md) (voided, it rested on misidentified
resistors) and the handover's original 800 Ω / 1.72 kΩ, ruled out by
[ADR-005](DECISIONS.md).

⚠️ The schematic for that last design was renamed on 2026-08-03 to
[hardware/plano_divisores_v1_NO_MONTAR.svg](hardware/plano_divisores_v1_NO_MONTAR.svg),
because it coexisted with the current one under a similar name and the risk of
opening the wrong one right before building was real. **It is not built for two
independent reasons:** the values are ruled out by [ADR-005](DECISIONS.md), and
it also sends the divider to the **yellow** wire, which turned out to be the
5 V, leaving the clock unconnected.

A redirect file remains under the old name, because ADR-005 links the schematic
by name and ADRs are not edited.

**There is only one current schematic:
[plano_sonda_v2.svg](hardware/plano_sonda_v2.svg).**

---

## Relay actuation timing

From the datasheet, and these bound a relay pulse on both sides:

| Limit | Value | Where it comes from |
|---|---|---|
| **Minimum** for the box to see the press | **~160 ms** | The chip requires it to last at least two scan periods, and the period reaches 80 ms [datasheet] |
| **Maximum** on M1/M2, before it stores the preset | **3.0 s** | **Measured on the bus on 2026-08-06** |

A pulse that is too short does nothing and looks like a wiring fault. One too
long on M1 or M2 overwrites the preset silently.

### The storage threshold, measured, VERIFIED

*Capture
[2026-08-06-umbral-grabar-memoria.log](capturas/2026-08-06-umbral-grabar-memoria.log).
It was measured by holding down the preset whose stored value **already equalled
the current height**, so that storing destroyed nothing.*

Counted on the bus, not with a stopwatch: the box reads the keyboard every
~200 ms, so that is the resolution.

```
16.781 s   first read with the key pressed
   ...     15 consecutive reads, display fixed at '080'
19.778 s   the display goes off for the first time  <- IT HAS STORED
20.378 s   blink
20.977 s   blink
21.576 s   blink
22.175 s   blink
```

**From the start of the press to the first blink: 2.997 s.** The storage
confirmation is **five blinks every 0.6 s**, and they continue even after the
button is released.

### Height range, VERIFIED

*Capture
[2026-08-06-topes-fisicos.log](capturas/2026-08-06-topes-fisicos.log), running
to both mechanical stops.*

| | |
|---|---|
| Bottom stop | **73 cm** |
| Top stop | **118 cm** |
| Travel | **45 cm** |

**Nothing special happens at the stop:** the display keeps showing a number,
does not blink, no new command appears on the bus, and it barely slows, holding
~1.2 s per centimetre to the end. **Reaching the stop is indistinguishable from
standing still**, except that the height stops changing.

These are the numbers for the phase 4 software limits
([SEGURIDAD.md](SEGURIDAD.md)).

### Up and down: tap versus continuous travel, VERIFIED

*Capture
[2026-08-06-umbral-toque-vs-continuo.log](capturas/2026-08-06-umbral-toque-vs-continuo.log).*

Up and down have **two regimes**, and only one is safe:

| | What it does | Does it stop by itself? |
|---|---|---|
| **Short tap** | Moves ~1 cm | **Yes** |
| **Hold and release** | Starts continuous travel | **No.** It carries on until any button is pressed |

**The threshold is between 2.2 and 2.6 s**, bracketed by pressing for
progressively longer:

| Duration | Result |
|---|---|
| 200 ms · 1.0 s · 1.6 s · 1.8 s | Tap |
| **2.2 s** | Tap, the longest that stopped by itself |
| **2.6 s** | **Continuous**, the shortest that triggered it |
| 2.8 s | Continuous |

⚠️ **Releasing does not stop continuous travel.** Measured: it carried on **5 cm
in 5 s** rising and **6 cm in 6.6 s** descending after release. Stopping it
requires **closing** a contact, not opening one.

**Corrected on 2026-08-21:** this sentence used to say *"it only stopped on
pressing **another** key"*, while the table above said *"any button"*. The
imprecise one was this. **Measured descending, with a single channel wired: a
short tap on the SAME button stops its own continuous travel.** It stopped at
76 cm and stayed still for 8 s. It matters because, if a different button were
required, a single channel could never brake. See [ADR-028](DECISIONS.md).

**The taps that "move ~1 cm" are 1.0 s or longer.** The shortest one timed on
2026-08-06 was 200 ms and **how far it moved was not recorded**. Measured on
2026-08-21: **at 300 ms the desk does not move**, seven pulses, zero
centimetres, although the box does register the key. At **800 ms** it descends
close to 1 cm every three pulses. See [ADR-027](DECISIONS.md).

**The display blinks when continuous travel starts.** Observed by eye and
confirmed on the bus: 0 blinks after the 2.2 s tap, 2 and 3 after the continuous
ones. It gives the firmware a way to detect a movement it did not request.

### Working window for actuation

| | Duration | Source |
|---|---|---|
| Minimum for the chip to see it | **160 ms** | Two scan periods [datasheet] |
| **Chosen pulse width** | **800 ms** | [ADR-027](DECISIONS.md), correcting the 300 ms of [ADR-023](DECISIONS.md) |
| Long pulse, to start continuous travel | **2800 ms** | [ADR-028](DECISIONS.md) |

### Summary of the four measured thresholds

⚠️ **Rebuilt on 2026-09-02.** These rows existed as an orphan table fragment,
severed from its header and stranded mid-section after an unrelated paragraph,
so they rendered as loose text. No figure has been changed.

| | Duration | Source |
|---|---|---|
| Recall a preset (tap) | < 0.2 s | A single keyboard read |
| Tap → **continuous travel** | **2.2 to 2.6 s** | Measured above |
| Tap → **store preset** | **3.0 s** | Measured above |

**Both thresholds land around 2 to 3 seconds**, so the box seems to have a
single notion of "long press" and apply it identically to all four buttons.

**The design that follows from this ([ADR-023](DECISIONS.md)):** the firmware
**only emits taps**, and a hardware pulse-width limiter cuts any pulse at
**300 ms** on all four channels.

```
160 ms          300 ms                      2200 ms
 |---------------|---------------------------|
 chip's        chosen                     danger
 minimum                                  starts
```

With that, a stuck contact **cannot start continuous travel or overwrite a
preset**: at worst it moves a centimetre or recalls a memory.

The chip also supports **combinations of KI1 and KI2 on the same DIG**, with top
priority. If any handset function uses a combination, a single relay cannot
reproduce it. The capture will tell.

---

## GPIO selection

The lines go to **P18** (CLK, red) and **P4** (DIO, green), with a common
**GND**. **Both verified present on the terminal block**, see the map below.

**P18 was chosen so as not to depend on which module this board carries**
([ADR-020](DECISIONS.md)). The previous assignment was P16, and on **WROVER**
modules GPIO 16 and 17 are wired to the PSRAM and are unusable.

### Terminal block map, VERIFIED by photograph

*Expansion board `FOR ESP32 TERMINAL ADAPTER`, read off the product photograph
with the silkscreen horizontal and legible.*

| Left column | Right column |
|---|---|
| CLK | 5V |
| SD0 | GND |
| SD1 | SD3 |
| P15 | SD2 |
| P2 | P13 |
| P0 | GND |
| **P4** ← DIO, green | P12 |
| P16 | P14 |
| P17 | P27 |
| P5 | P26 |
| **P18** ← CLK, red | P25 |
| P19 | P33 |
| **GND** ← the handiest one | P32 |
| P21 | P35 |
| RX | P34 |
| TX | SVN |
| P22 | SVP |
| P23 | EN |
| GND | 3V3 |

**How to find them without depending on the board's orientation:** from **P4**,
counting in the direction away from P0, come **P16, P17, P5 and P18**. Two
positions past P18, beyond P19, there is a **GND**, and that is the handiest one
for the divider returns, more so than the corner one.

**Do not touch `CLK`, `SD0`, `SD1`, `SD2` or `SD3`.** They are GPIO 6 to 11,
wired to the module's internal flash. They are brought out to the terminal block
but using them stops the ESP32 booting. They join the boot pins (0, 2, 5, 12,
15) on the list of what is never touched.

### The module

**Data read off the chip itself by esptool on 2026-08-03**, during an upload. It
is the best evidence available: reported by the silicon, not by a label or a
seller.

| | |
|---|---|
| Chip | **ESP32-D0WD-V3**, revision **v3.1** |
| Cores | Dual core + LP core, **240 MHz** |
| Crystal | 40 MHz |
| Features | Wi-Fi, BT, Vref calibration in eFuse, *Coding Scheme None* |
| MAC | `b4:bf:e9:0f:06:9c` |
| Usable flash | ≥ 1,310,720 bytes of program, 327,680 of RAM |
| Arduino core | esp32 **3.3.11** |

*The D0WD-V3 is the chip in both the WROOM-32E and the WROVER-E, so this does
**not** by itself decide which module it is. It does not matter: since
[ADR-020](DECISIONS.md) the project does not depend on that distinction.*

Shield silkscreen: **`ESP-32`**, with CE marking, `ISM 2.4G 802.11 b/g/n` and
**`FCC ID: 28B77-ESP32-32X`**. **CP2102** USB-serial from Silicon Labs.

- **Verified:** it carries no `WROVER` marking anywhere, and the format is the
  **short** one: the shield takes up little more than half the board, with room
  to spare underneath for the USB-C, the two buttons and the regulator. A WROVER
  is 6 mm longer and would not leave that gap. It is a **WROOM-32 class module,
  with no PSRAM**.
- **Assumed:** that it is a genuine Espressif WROOM-32. The `ESP-32` marking and
  that FCC ID are not those of a catalogue Espressif module; it is a compatible
  one. For this project it makes no difference: **with CLK on P18 the
  distinction has no consequences**, which is why it was decided that way.
- Practical consequence of the CP2102: the port will show up as
  `/dev/cu.usbserial-XXXX`. macOS has shipped the driver since Big Sur, nothing
  to install.
- The boot GPIOs are avoided on purpose: **0, 2, 12 and 15**. An unexpected
  level on any of them during reset leaves the ESP32 unable to boot, or booting
  into flash mode.
- The GPIO connects **to the middle node** of the divider, never to an end.
- All the grounds go together: divider returns, blue wire and ESP32 GND. Without
  a common reference there is no valid measurement.

---

## Power

ESP32 over **USB from the Mac**.

⚠️ **Corrected on 2026-09-02.** This line used to read *"the desk's red wire is
not connected"*. **Wrong, and in the one place it is most dangerous to be
wrong.** Red is CLK and it *is* connected, through its divider. The wire that is
never connected is the **yellow** one, which is the 5 V. The rest of this
document says so correctly in four separate places, including the pinout table
and the probe schematic; this single line contradicted all of them, and it was
the last line of the file.

See [ADR-007](DECISIONS.md).
