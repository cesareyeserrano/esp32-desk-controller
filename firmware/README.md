# Firmware

> *Documento en inglés desde el 2026-09-02. La bitácora y los ADR siguen en
> español; ver [POLITICA_DOCUMENTACION.md](../docs/POLITICA_DOCUMENTACION.md).*

## `desk_sniffer/`

A passive sniffer for the handset bus. **It only listens, it never writes.** Both
pins stay configured as inputs from beginning to end, which is what
[ADR-011](../docs/DECISIONS.md) requires.

### Before connecting it

1. Dividers fitted on red and on green: **9.1 kΩ upper, 27 kΩ lower**
   ([ADR-016](../docs/DECISIONS.md)). Schematic in
   [HARDWARE.md](../docs/HARDWARE.md).
2. Taps soldered to the handset connector **with the original connector in
   place**.
3. **Check that the handset still works** with the taps soldered on, before
   connecting the ESP32. Skip that order and any later fault becomes impossible
   to attribute.
4. **CLK goes to P18 and DIO to P4**, both in the same terminal column
   ([ADR-020](../docs/DECISIONS.md)). From P4, moving away from P0: P16, P17,
   P5, P18. The handiest GND is two positions past P18, beyond P19. Full map in
   [HARDWARE.md](../docs/HARDWARE.md). **Watch out for `CLK`, `SD0`, `SD1`,
   `SD2` and `SD3` on that same terminal block: they are the internal flash and
   using them stops the chip booting.**
5. **ESP32 powered over USB from the Mac, and powered BEFORE connecting the
   wires to the divider.** With the ESP32 unpowered and the probe on a live bus,
   the bus sags to about 2.85 V and the handset fails
   ([ADR-019](../docs/DECISIONS.md)). When taking it apart, wires first and USB
   second. The yellow wire is never connected.
6. **Measure the bus level, and judge it by the ratio**, not the absolute value:
   a multimeter averages and the bus is switching. Measure red to blue and green
   to blue without the probe and then with it, desk still and the same number on
   screen. The ratio should sit around **0.80**; below **0.70**, disconnect.
   Full table in [HARDWARE.md](../docs/HARDWARE.md), reasoning in
   [ADR-018](../docs/DECISIONS.md).

### Building and flashing

Arduino IDE with ESP32 support installed:

- Board: **ESP32 Dev Module**
- Port: **`/dev/cu.usbserial-XXXX`**. The board carries a **CP2102** USB-serial
  chip, for which macOS has shipped a driver since Big Sur, so nothing needs
  installing. Confirm with `ls /dev/cu.*` before and after plugging it in.
- Open `desk_sniffer/desk_sniffer.ino` and upload

**Why CLK is on GPIO18 and not GPIO16.** An earlier version of this file claimed
the module is a WROOM-32. **That was never checked**: the shield is not legible
and all we have is the seller's listing. And on **WROVER** modules, GPIO 16 and
17 are wired to the PSRAM and are unusable.

Rather than settle the question it was removed: **GPIO18 and GPIO4 are free on
both modules**, are not boot pins, and are not flash pins. "WROOM or WROVER?" no
longer has consequences here. See [ADR-020](../docs/DECISIONS.md).

**Compiled and run for the first time on 2026-08-03.** ✅

Arduino IDE with the Espressif core, board *ESP32 Dev Module*, on the real
DevKit. **It compiles cleanly, uploads and boots.** The line self-check runs and
reports correctly, with the ESP32 powered by USB alone and **nothing connected
to the desk**:

```
Identifying lines (2 s)...
  GPIO18 (expected CLK, red)  : 0 edges
  GPIO4 (expected DIO, green): 0 edges
  !! No activity on either line.
```

Zero edges on both lines is **the expected result with nothing connected**.

Noise was then injected by touching GPIO18 with a finger, counters reset:

```
  edges captured : 197548
  edges dropped  : 0
  transactions   : 0 (0 malformed, 0 ended by repeated START)
```

**What that demonstrates:** that it compiles with the real toolchain, that it
boots, that serial at 921600 works, that interrupts attach and fire, that the
queue absorbs ~200,000 edges **without losing one**, and that the decoder **does
not invent transactions out of noise**, since START/STOP detection does not
hallucinate protocol where there is none.

**What it does not:** the decoder against real data. That needs a bus, and that
was phase 2.

**Debugging tip:** the window for command `l` is 2 seconds, and edges have to
already be happening when it is pressed. To check a pin without rushing: `c`,
generate activity, `s`, and look at `edges captured`.

## ⚠️ Which counter to watch: `malformed`, not `dropped`

Measured on 2026-08-03 with
[test_capture_ceiling](test_capture_ceiling/test_capture_ceiling.ino):

| Square wave | Edges expected | Captured | `dropped` |
|---|---|---|---|
| 125 kHz | 250,000 | 250,000 | 0 |
| **150 kHz** | 300,000 | **299,613** | **0** |

**At 150 kHz edges are lost and `dropped` reads zero.** The cause: when two
edges arrive closer together than the ISR takes to run, the GPIO status register
records *that* an interrupt happened but not *how many*. The two collapse into a
single call and the second vanishes without incrementing any counter.

`dropped` only counts edges that **reached the ISR and did not fit in the
queue**. It cannot see the ones that never fired it.

**Operating rule:** `dropped` at zero **is not** proof of healthy capture. The
canary is **`malformed`**: an edge lost mid-transaction throws off the bit count
and flags it. That one does catch silent loss.

### Correction: `malformed` never reached zero with burst sampling

It was claimed that moving to bursts made malformed transactions disappear.
**That was not so:** a stubborn **0.7 to 1%** remained across every capture of
2026-08-06, and **all 171 of them had exactly zero bytes**.

They were not broken traffic. They were **ours**. Between one burst and the next
the bus keeps moving with nobody watching, and the decoder, whose state is
global, compared the first change of the new burst against a level from
milliseconds earlier, inventing a START or a STOP at the seam.

Fixed: `decodeBurst()` **reseeds the decoder** with the first sample of every
recording and discards whatever it thought it had half-finished, counting that
separately under `cut by burst`. So **`malformed` means "the bus said something
we could not read"** again, instead of measuring our own instrument.

That is the third time in this project a counter measured the tool rather than
the world. Distrusting them by default is worth the effort.

**But it did not reach zero: the noise floor is ~0.8%.** Across 15 minutes of
idle, 175 malformed out of 22,731 transactions remained, all of the `S P` kind
with no bytes, spread irregularly and with `cut by burst` at zero.

**They are not losses, they are extra transactions.** The proof is that the
cycle count works out exactly: 18,020 digit writes ÷ 4 = 4,505 cycles = 4,505
keyboard reads. No cycle arrived incomplete.

*Unverified hypothesis:* at 4 MHz each sample lasts 250 ns, so if DIO changes
inside that window relative to the CLK falling edge, what gets seen is a DIO
change with CLK still high, which by definition is a START or a STOP. That would
be sampling resolution. If it ever gets in the way, the route is to require CLK
to hold steady for several samples before declaring a START or STOP.

**When reading `malformed`, the reference zero is ~0.8%, not 0.**

### And before believing any counter that reads zero

The test sketch was born with its success criterion set to `dropped == 0`, and
**it reported "clean up to 400 kHz" with the cable unplugged**: with no signal
there is nothing to discard, so the failure counter stays at zero and everything
looks perfect. Fixed on 2026-08-03, and it now requires the expected edges to
have actually arrived.

It is the same trap the review of 2026-08-02 found in `checkLines()`, which
reported OK with a cut cable. **A zero error counter can mean "all good" or "no
one is there."** When reading any phase 2 capture, before trusting a statistic:
check there was traffic to count.

**Measured ceiling:** clean up to **125 kHz square wave = 250,000 edges/s**.
Since edges arrive at twice the bus clock frequency, that covers a bus up to
about 125 kHz. This chip family usually runs well below that.

*Before this there had only been a syntax pass with clang and simulated headers
(`-fsyntax-only -Wall -Wextra`, zero warnings), which ruled out typos and type
errors but not mismatches against the real core signatures.*

### Over-the-air updates, since 2026-08-23

**The ESP32 lives in a wall charger, so the cable is not needed.** The IP is
published as the `IP` entity in Home Assistant (currently `192.168.1.23`), and
the password lives in `secrets.h`.

```
CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
"$CLI" upload -p 192.168.1.23 --fqbn esp32:esp32:esp32 \
  --upload-field password=<OTA_PASSWORD> firmware/desk_sniffer
```

⚠️ **The update is refused while the desk is moving**
([ADR-034](../docs/DECISIONS.md)): rebooting opens the channels, and opening a
contact does not stop continuous travel.

A `[WARNING]: Unexpected response from device: '256'` at the end is cosmetic,
from the upload script. If `100% Done` appeared, the update was applied, which
can be confirmed by watching the `uptime` in HA start over.

**The cable is still needed for:** the first flash of a new board, and reading
the serial port.

### Serial monitor

**115200 baud.** *(Corrected twice on 2026-08-21: it said 921600, moved to
460800 and ended up here. At 921600 the board transmits fine but receives not a
single command; at 460800 it did not receive either, despite what
[ADR-025](../docs/DECISIONS.md) says. **The cause of the receive failure remains
unidentified**, and the full table of what was ruled out is in
[ADR-026](../docs/DECISIONS.md).)*

⚠️ **The argument was corrected too, because it was wrong.** This paragraph used
to say that *"the display refreshes every ~8 ms, so at 115200 the port would be
the bottleneck"*. **Measured on 2026-08-21 against the captures: the refresh
cycle is 200 ms, not 8.** The five messages of a cycle go out in 1.5 ms and then
the line stays silent until the 200 ms mark.

Real idle throughput is **1.4 KB/s out of the 11.5 KB/s that 115200 provides**,
about 12%. Plenty of room, and the margin once believed necessary never was.

**Where it does get tight is with the raw dump (`r`) on**, which is the mode that
spits out the most. If lines start going missing there, the answer is not to
raise the baud rate blindly: first check that commands are still received at
that speed, by sending `h`.

To capture to a file:

```
screen -L -Logfile docs/capturas/2026-08-02-reposo.log /dev/cu.usbserial-XXXX 115200
```

Leave `screen` with `Ctrl-A` then `K`.

After capturing, **add the context header to the file**, format in
[capturas/README.md](../docs/capturas/README.md). A capture without context is
useless.

### What it does at boot

It counts edges on both lines for 2 seconds and says whether the wiring makes
sense. CLK carries a burst of pulses per transaction and DIO changes at most
once per bit, so if DIO shows more edges than CLK, the wires are crossed. That is
the most likely wiring mistake and it produces garbage that looks like a
protocol problem.

If it sees no activity on either line: check GND, the dividers, and that the
desk is powered on.

### Output format

```
[     3.482910] S 68a 07a P  | DIG1 seg=0x07 '7'
[     3.483514] S 6Aa E6a P  | DIG2 seg=0xE6 '4' +DP
[     3.484102] S 6Ca 6Da P  | DIG3 seg=0x6D '5'
[     3.484688] >>> DISPLAY: "74.5"
[     3.520044] S 49a 2E- P  | KEY none
```

- `S` and `P` are START and STOP.
- Every byte carries its ACK: `a` = acknowledged (line low), `-` = line high.
- **On keyboard reads, the `-` on the second byte is normal**, not a fault: the
  datasheet says that in a read the ninth bit of the command is 0 and that of
  the data is 1. On writes both bytes should come out with `a`.
- The `>>>` lines are events: a change in what the screen shows, or a key press.
  They appear even with the raw dump off.

### Serial commands

| Key | Effect |
|---|---|
| `r` | Toggles the raw dump of every transaction |
| `s` | Statistics: edges, transactions, bus speed, discards |
| `c` | Resets the statistics |
| `l` | Repeats the line check |
| `h` | Help |

### What to look for in the statistics

**`edges dropped`** means the buffer filled and edges were lost. Careful with
the interpretation: **it can be our own fault**. With the raw dump on, printing
every transaction can saturate the serial port and block the loop while the
interrupt keeps filling the queue. To find out whether the bus really runs
faster than we capture, **turn the dump off with `r` and look again**. The
statistics message itself says so.

**`malformed`** counts transactions that do not carry exactly two bytes. The
datasheet says all of them are 16 bits, so anything else is a lost edge. A few
when starting to listen are normal, since you join mid-transaction; many and
sustained are not.

**`ended by repeated START`** counts transactions that ended with another START
instead of a STOP. The datasheet does not describe that case, so if it shows up
systematically the master is doing something unexpected and it needs looking at
before trusting the decoding.

**`fastest clock`** is the figure that decides whether the resistive divider
holds up. The impedance that governs is the one on the **rising edge**, which on
the [ADR-016](../docs/DECISIONS.md) probe is **10.9 kΩ**, not the 6.8 kΩ this
file used to claim. That value only applies to the falling edge
([ADR-018](../docs/DECISIONS.md)). It is still better than earlier probe
versions, but with less margin than was believed.

There is no kHz threshold that can be given by calculation, because it depends
on the parasitic capacitance of the build, meaning wire length and breadboard,
which has not been measured. **The criterion is empirical:** if `malformed`
stays at zero and the bytes decode, the divider works at whatever speed is
present. If bytes go missing persistently with the raw dump off, the remedy is
the 74LVC2G17 buffer declared in [ADR-018](../docs/DECISIONS.md), and **never**
lowering the resistor values, which loads the bus.

It is measured in CPU cycles, only inside a transaction, and the count is reset
at every START, so that neither the dead time between transactions nor a counter
overflow can falsify it.

### Adversarial review

The code went through a fault-hunting review on 2026-08-02, which found eight
issues. The two serious ones: the cycle counter overflowed after 17.9 s of bus
silence, which is exactly the [ADR-012](../docs/DECISIONS.md) case of the
handset sleeping, and the line check reported "OK" with a cut cable. Both fixed.
Detail in the [log](../docs/BITACORA.md).

A second review on 2026-08-03 found two more, both descendants of the same
17.9 s overflow the first one thought closed:

1. **The minimum clock period could be poisoned.** The count was reset per
   transaction according to the comment, but not according to the code: the
   first edge of each frame measured backwards into the previous frame, across
   the dead gap. If that gap exceeded 17.9 s, with the chip asleep, the
   subtraction wrapped around and left a false, tiny minimum in the one number
   that decides whether the divider works.
2. **The timestamp could jump 17.9 s forward.** If the interrupt queued an event
   just after the loop had declared the queue empty, the keep-alive advanced the
   base past that event and the unsigned subtraction turned it into an enormous
   jump, permanent from then on.

Both fixed. Neither could damage anything, since the firmware still never writes
to the bus, but both corrupted capture data silently, which is the failure mode
this project can least afford.

### Verification status

⚠️ **Corrected on 2026-09-02.** This section used to read, in full: *"What is
unverified: everything. This firmware is written against the datasheet, not
against the real bus. It has never been run with hardware connected. The seven
open questions in PROTOCOLO.md are still open."*

**All of that stopped being true weeks ago** and nobody updated it. The firmware
has been running against the real desk since 2026-08-06, the bus is decoded, and
five of those questions are answered (see
[PROTOCOLO.md](../docs/PROTOCOLO.md)). The paragraph survived because nothing
forces a document to be re-read when the thing it describes changes.

**Current status:** running in production on the real desk, reading height,
driving all four channels, and reporting to Home Assistant over MQTT. What
remains unverified is listed in [PLAN.md](../docs/PLAN.md) and in
[SEGURIDAD.md](../docs/SEGURIDAD.md), the outstanding item being the 74HC14
buffer, which is designed but has never been built.

---

# test_output_channels: checking the actuation

Tests the four actuation channels **before anything is soldered to the
handset**.

**None of this touches the desk.** The optocouplers sit on the breadboard with
their output side **connected to nothing**, so the worst this sketch can do is
light an infrared LED inside a chip.

## What it proves, in order of importance

1. **That the pins are low at reset and stay low through boot.** All of
   [ADR-024](../docs/DECISIONS.md) rests on this: the watchdog protects us by
   rebooting the chip, and a reboot only helps if it leaves the channels open.
   If a pin went high at boot, the watchdog would **press** a button instead of
   releasing it, the exact opposite of the protection.
2. That a commanded pulse lasts what it should and not a millisecond more.
3. That two channels are never active at once.

## Wiring, per channel

**Schematic: [plano_canal_pc817.svg](../docs/hardware/plano_canal_pc817.svg).**

```
GPIO --[330 ohm]--|>|-- PC817 pin 1 (anode)
                        PC817 pin 2 (cathode) -- GND
  |
[10k]      pull-down: without it, a weak internal pull-up at boot (~45 kOhm)
  |        would push ~47 uA through the LED, uncomfortably close to the ~90 uA
 GND       the handset needs to see a key. With it, that leakage sits at 0.6 V,
           well under the 1.2 V the LED needs to conduct.
```

Multimeter **on pins 3 and 4 of the PC817** to watch the channel.

## Pins

**GPIO 27, 26, 25 and 33**, four in a row on the right-hand terminal block. None
is a boot pin (0, 2, 5, 12, 15), none belongs to the flash (6 to 11), and all
four can be outputs, unlike P34, P35, SVN and SVP in that same column, which are
**input only**.

## What should happen

| Moment | Multimeter on pins 3-4 |
|---|---|
| EN held down, chip in reset | **Open**, megohms |
| Releasing EN, during boot | **No beep** in continuity mode |
| Idle after boot | **Open** |
| While pressing (key `1`) | **Conducts** briefly |

Commands: `1` `2` `3` `4` to press a channel, `l` to see the level of all four
pins, `h` for help.
