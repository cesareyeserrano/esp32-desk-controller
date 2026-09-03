# Plan

> Phases and status. Updated when a phase opens or closes, or when the scope
> changes. The reasoning behind each decision lives in
> [DECISIONS.md](DECISIONS.md).
>
> *Documento en inglés desde el 2026-09-02. La bitácora y los ADR siguen en
> español; ver [POLITICA_DOCUMENTACION.md](POLITICA_DOCUMENTACION.md).*

**Current phase: 4, Home Assistant.** Phase 3 closed on 2026-08-22 with all four
channels verified against the bus. Phase 2 closed on 2026-08-06: the protocol is
decoded and the height is read off the bus, verified against the handset screen.

⚠️ **Tidied on 2026-09-02, and it needed it.** This file had accumulated
unchecked boxes for work finished a month earlier ("solder the taps ← we are
here", with the taps soldered since 2026-08-06), a summary table still giving the
pulse width as 300 ms when [ADR-027](DECISIONS.md) raised it to 800, and a
sentence severed mid-clause. Nothing has been deleted: the phase history is
below, with the boxes now reflecting reality. **The stale entries are called out
where they were wrong** rather than quietly ticked.

---

## Where things stand

Everything essential in phase 4 works: 21 entities in HA, travel with limits and
verified braking, automatic give-way to the handset, monitoring with phone
alerts, over-the-air updates, and the public repository on GitHub.

✅ **Posture reminders work end to end**, verified on 2026-09-02 at 12:30: it
warned, waited, re-checked presence and **raised the desk from 80 to 117 with no
manual intervention**. That is the first thing the system does usefully on its
own.

✅ **Adversarial review of 2026-08-23: 16 findings, 16 fixed and verified live**
([REVISION_FIRMWARE_2026-08-23.md](REVISION_FIRMWARE_2026-08-23.md)). Among
them: the [ADR-024](DECISIONS.md) watchdog that had never been implemented,
three HA buttons that never worked, the transient-height filter, limits in both
directions, verified braking with retry, and `parar` on its own channel so it
cannot be lost. A copy of the previous firmware is in `firmware/backups/`.

## Next concrete steps

**Step 5, trusting the reminders end to end. ⬜ IN PROGRESS since 2026-09-02.**

Phase 4 is built and running, but **the reminders have failed silently three
times** (2026-08-24, 08-31 and 09-02), each time for a different reason and
**none of them visible from the Home Assistant interface**. The last two, the
presence sensor flickering and the counter being wiped on every restart, are
fixed and verified on 2026-09-02.

⚠️ **A fourth failure on 2026-09-03, and the worst kind: the desk rose with
nobody in the room.** It was caused by the previous day's fix, which moved the
pre-movement check onto a sensor with 15 minutes of inertia. Fixed with a
separate 3-minute sensor for that one question, plus covering `unavailable` in
the mid-travel watch. Detail in [INTEGRACION_HA.md](INTEGRACION_HA.md).

What remains is to **watch a couple of days of real use**, and to watch **both
directions**: that reminders end in movement, *and* that no movement happens
without somebody there. Every fix so far has been verified against the symptom
that motivated it and not against what it broke the other way. That is how the
2026-09-03 failure got in.

⚠️ **And there is a second open matter, a different one:** on 2026-09-02 **nine
warnings fired** (07:15, 07:45, 08:20, 08:55, 09:30, 10:50, 11:25, 12:00 and
12:30) and the owner **saw no notification at all**. The notification is sent
before the 110 s wait, so they were sent. **Whether they reached the phone could
not be checked, because the logs for that window had already rotated.**

**On resuming, look at this first**, with fresh logs: if the
`notify.mobile_app_icesar_pro` channel is not delivering, the system can move
the desk without warning you first, which is worse than not moving it.

**How to check without guessing:** the `recorder` database queries are in
[INTEGRACION_HA.md](INTEGRACION_HA.md). That section exists because on
2026-09-02 a whole session was spent issuing plausible and wrong diagnoses with
30 days of records sitting there unread.

**Step 5b, deciding about the 2800 ms window.** The long pulse cannot be aborted
and, if the opposite key is pressed while it lasts, the box sees UP and DOWN at
once. **What it does with that is not verified.** The check is cheap, pressing
both on the handset with the ESP32 disconnected, and it decides whether a fix is
needed. Detail in [SEGURIDAD.md](SEGURIDAD.md). If the decision is to use M1 and
M2 for the posture targets, **an ADR is required**: it contradicts a decision
already taken.

**Step 6, the board.** Specification and schematics ready in
[hardware/PCB_ESPECIFICACION.md](hardware/PCB_ESPECIFICACION.md). Two things
**before** ordering anything:

1. **Identify the original cable connector** (pitch, latch, marking). Without
   that the JST cannot be chosen.
2. **Test the 74HC14 buffer on breadboard.** It goes on the board and has never
   been built. It is the main reason for rebuilding the board
   ([ADR-031](DECISIONS.md)): the breadboard was the fragile link of the whole
   project.

**Step 7, layer 1 of electrical protection.** The ESP32 charger and the desk on
the same power strip. It costs nothing and is still pending
([SEGURIDAD.md](SEGURIDAD.md)).

**Then, by value:** named presets ("standing", "sitting"), which is configuration
on top of what already works; usage statistics in HA, where the threshold in cm
still has to be chosen, plus InfluxDB for long-term history; and the minor loose
ends, meaning the unidentified idle bytes and the chip photographs still to be
archived.

### Rules that do not change

⚠️ **USB first, bus wires second** ([ADR-019](DECISIONS.md),
[ADR-031](DECISIONS.md)), and **send `h` and check it answers before trusting
any channel test** ([ADR-026](DECISIONS.md)).

### Known and unexplained

- **The handset came and went, and nobody knows why (2026-08-23).** It stopped
  working and came back at least five times in one day, always around handling
  it. Four explanations were proposed and **none was verified**. Detail in the
  [log](BITACORA.md). ⚠️ **While it comes and goes, no test is worth anything**:
  a result on an intermittent build cannot tell a real fault from a
  coincidence, and that is how two days were lost.
- **The five idle keyboard bytes**: `0x07`, `0x17`, `0x27`, `0x2E`, `0x2F`.
  **None carries the `0x40` bit**, so none is a pressed key and they get in the
  way of nothing. They probably indicate which column is being scanned.
- ⚠️ **The Zigbee sensor delay is at 30 s.** It does not block anything, since
  the real protection is `binary_sensor.escritorio_presencia_sostenida`. Mostly
  explained on 2026-09-02, see [INTEGRACION_HA.md](INTEGRACION_HA.md); one row
  of that table is still unexplained.

### Resolved, kept because they cost time

✅ **2026-08-23: the handset was healthy.** What looked like a damaged chip was a
**short between the green wire (DIO) and the yellow one (5 V)**, a solder bridge
made while closing the case. With the short removed, the handset worked
normally.

✅ **Channel 3, resolved 2026-08-23: it was a loose optocoupler on the
breadboard.** Seated properly, it answers `0x67` first time.

---

## Physical state of the build

Everything assembled and running since 2026-08-06:

| | |
|---|---|
| Taps | Three wires soldered to the handset's JST connector, original connector left in place. The yellow one was **not** soldered |
| Probe | On breadboard, 9.1 k + 7.4 k on top and 27 kΩ below, per channel ([ADR-022](DECISIONS.md)) |
| Connection | P18 = CLK (red), P4 = DIO (green), common GND |
| ESP32 | On a wall charger since 2026-08-23, running the **burst sampling** build of `desk_sniffer` |
| Levels | Bus 4.7 V, GPIO node 2.9 V |
| The handset | **Works normally** with everything connected |

**If it has to be disconnected and reconnected:** USB first, bus wires second
([ADR-019](DECISIONS.md)); reverse order when taking it apart. And the divider is
only checked **with the blue wire out**, or the handset provides a parallel path
of ~34 kΩ and the number means nothing.

## How to capture

The sniffer dumps over serial at **115200**. *(It was 921600, then 460800, and
ended at 115200 on 2026-08-21: it is the only speed at which commands have been
verified end to end as received while the bus reads correctly. **The receive
failure is unexplained**, see [ADR-026](DECISIONS.md).)*

To record to a file without fighting the IDE's Serial Monitor, which has to be
closed first because only one program can hold the port:

```
exec 3</dev/cu.usbserial-0001
stty -f /dev/cu.usbserial-0001 115200 raw -echo
cat <&3 >> docs/capturas/YYYY-MM-DD-description.log
```

⚠️ **That is for listening only.** To *send* commands (`1` to `4`, `h`, `s`) you
need a single read-write descriptor with the speed set by the native macOS
ioctl; with shell redirection the bytes never arrive. Ready-made script in
[../tools/serial_talk.py](../tools/serial_talk.py).

The descriptor is opened **before** setting the speed: the other way round,
macOS resets the configuration on open and the output is unreadable.

A context header is mandatory on every capture, format in
[capturas/README.md](capturas/README.md).

## Loose tasks

- [x] **Time the preset-store threshold, 3.0 s.** Measured on the bus on
      2026-08-06 without risking any preset ([ADR-010](DECISIONS.md))
- [x] **Install the Arduino IDE, compile and flash the sniffer**, done
      2026-08-03
- [x] **Which preset is M1 and which is M2**, resolved 2026-08-22.
      **M1 → channel 3 → `0x67` → 80 cm. M2 → channel 4 → `0x6F` → 117 cm.**

      *Two different sources, and they are worth keeping apart:* the
      **channel → height** relation is **measured on the bus**, since channel 3
      ran 073→080 and channel 4 ran 081→117; the **button → channel** relation
      comes from whoever soldered the wires, because the handset's label is not
      visible from the bus. Both matched what he predicted before testing.

      *⚠️ A severed sentence used to sit here, reading in full: "physical is
      which. Trivial, and only needed when wiring." Its beginning was lost in
      some earlier edit and what remained said nothing. Removed on 2026-09-02;
      recorded rather than silently dropped.*
- [ ] Archive the macro photographs of the chip and the silkscreen in
      [hardware/fotografias/](hardware/fotografias/), which already has an index
      of which are missing
- [ ] Identify the **five idle bytes**, listed above

## Pending decisions

*None open right now.*

Closed on 2026-08-06:

- ~~Whether up and down get bounded in hardware~~ → **Yes, and with the same
  circuit as M1 and M2** ([ADR-023](DECISIONS.md)). The measurement revealed
  that up and down have two regimes and that the dangerous one, continuous
  travel, **cannot be stopped by opening a contact**, only prevented from
  starting.

Closed on 2026-08-03:

- ~~Whether to buy the logic analyser~~ → **No.** The project's founding
  constraint stands: the ESP32 acts as the instrument. It would only be bought
  if the capture came out dirty and could not be diagnosed blind. See
  [COMPRAS.md](COMPRAS.md).
- ~~What to use for actuation~~ → **Neither photoMOS nor mechanical relays to
  begin with:** PC817 first, the inventory relays if that does not fit
  ([ADR-021](DECISIONS.md)). PhotoMOS cost around $120,000 COP in Colombia and
  the cost premise of ADR-017 does not hold here.

---

## Phase 1, reconnaissance ✅

Identify the hardware and the intervention point.

- [x] Identify the handset: `JK-CH506 Rev1.2`, Jiecang
- [x] Identify the chip: AiP650EO, TM1650 family, no MCU in the handset
- [x] Verify the cable pinout with a multimeter
- [x] Confirm the buttons sit at 5 V
- [x] Inventory the resistors
- [x] Identify the function of the 5 buttons: up, down, M1, M2, reset
- [x] **Identify the 4 wires: red SCL, green SDA, blue GND, yellow 5 V.**
      Corrects the handover's assumption, which gave red as VCC
- [x] Work out how a preset is **stored**: by holding M1 or M2

### A. Identify the 4 wires by continuity to the chip ✅ DONE

**Result: red = SCL, green = SDA, blue = GND, yellow = VDD (5 V).** Verified at
0.2 Ω on 2026-08-02. See [HARDWARE.md](HARDWARE.md).

<details>
<summary>Procedure (already carried out)</summary>

With the connector unplugged from the control box, multimeter in **ohms** (not
continuity, since the beeper does not pass through series components and its
threshold can be permissive), measure each wire against the chip pins.

Numbering: pin 1 at the corner with the dimple, pin 16 opposite.

A reading of ~0.2 Ω is a direct trace. Anything above a few ohms is not a
connection, it is something else.

The pins are 1.27 mm pitch. Fine tip, and mind bridging neighbours.

</details>

### B. Measure the bus pull-up, NO LONGER NEEDED

**Resolved by the datasheet, not by measurement.** The pull-up is **internal to
the chip**: 550 µA typical, about 9.1 kΩ at 5 V. Any external resistors on top
would only make it stronger. See [ADR-013](DECISIONS.md).

*Measured anyway on 2026-08-23, from a different angle: with the handset
disconnected, the box shows 21 kΩ on CLK and 22 kΩ on DIO against the 5 V. See
[HARDWARE.md](HARDWARE.md).*

### C. Look at the control box from outside ❌ RULED OUT, there is no shortcut

**Checked by visual inspection on 2026-08-03. There is no accessory port.** All
the control box has is the 29 V adapter input, the 4-wire handset connector, and
the 6-wire motor cable. **No accessory RJ11, RJ12 or RJ45.**

The shortcut being sought, those Jiecang boxes with an extra port for Bluetooth
or a second handset, with a documented 9600 8N1 serial protocol and ready-made
ESPHome implementations, **does not apply to this desk**. That was to be
expected: a handset with an AiP650E/TM1650 indicates a budget-range box.

**Consequence: the plan stands exactly as it is.** The handset bus is the only
way in, and by the same token there is no longer an alternative left to explore.
References for the discarded shortcut in [REFERENCIAS.md](REFERENCIAS.md).

## Phase 2, sniffing ✅

Listen to the bus without disturbing it and decode the height.

- [x] Bus pull-up: resolved by datasheet, 9.1 kΩ internal
- [x] Design the probe: 15 kΩ / 33 kΩ divider ([ADR-013](DECISIONS.md))
- [x] Measure the whole resistor drawer with a multimeter, 30 pieces
- [x] Final probe defined: 9.1 kΩ / 27 kΩ ([ADR-016](DECISIONS.md))
- [x] Probe verification criterion corrected: judged by ratio, not absolute
      value ([ADR-018](DECISIONS.md))
- [x] **Obtain two 27 kΩ resistors**, received and measured 2026-08-03
- [x] **Solder the taps to the handset's JST connector (AWG 28)**
      *⚠️ This box was unticked and marked "← we are here" until 2026-09-02, with
      the taps soldered since 2026-08-06 and the desk running off them.*
- [x] CLK moved from P16 to **P18**, free on both WROOM and WROVER, so the
      module cannot be identified with certainty and it stops mattering
      ([ADR-020](DECISIONS.md))
- [x] **P18 confirmed on the terminal block**, with the full map of both columns
      recorded in [HARDWARE.md](HARDWARE.md)
- [x] **Verify the handset still works with the taps fitted, before connecting
      the ESP32** *(⚠️ also unticked until 2026-09-02; it was done on
      2026-08-06)*
- [x] Reference reading of the bus **without the probe**, for the ratio
      *(⚠️ likewise)*
- [x] Fit the probe on breadboard and **verify the bus level by ratio**
      *(⚠️ likewise; bus at 4.7 V, GPIO at 2.9 V)*
- [x] **Write the sniffer**, in
      [firmware/desk_sniffer/](../firmware/desk_sniffer/)
- [x] **Compile and flash the sniffer on the real ESP32**, done 2026-08-03
- [x] **Probe fitted and verified**, 2026-08-06. The real pull-up turned out to
      be 2.4 kΩ rather than 9.1 kΩ, which forced the divider to be redone
      ([ADR-022](DECISIONS.md))
- [x] **First bus capture**: the sniffer reads commands matching the datasheet,
      `48`, `6A`, `6C`, `6E`, `4F`
- [x] **Fix the framing.** The bus runs at ~202 kHz, above the ceiling of
      interrupt capture. A histogram of intervals ruled out double-counted
      edges. The sniffer moved to **burst sampling** at 4 MHz
- [x] Capture traffic with the desk idle (display refresh)
- [x] **Correlate bytes with the number on screen → height decoded.** Verified
      in four cases, including a three-digit one
- [x] Capture each button separately. **Up `0x47`, down `0x57`, presets `0x6F`
      and `0x67`.** Reset was not pressed and never will be
- [x] **Check whether the height refreshes during movement: YES.** Every
      centimetre, ~1.2 s. **Closed-loop control is viable**
- [x] **Check whether the chip ever sleeps: NO.** 15 minutes idle, 4505 arms and
      **not one with the bus silent**. Bounds [ADR-012](DECISIONS.md)
- [x] **Real range measured: 73 to 118 cm.** Hitting the stop produces nothing
      distinguishable from standing still
- [x] Document the protocol in [PROTOCOLO.md](PROTOCOLO.md)
- [x] **Speed confirmed: 8.5 mm/s**, ~1.2 s per centimetre, with a 2.5 to 3 s
      ramp over the first few centimetres

## Phase 3, actuation ✅

Read height over the bus, actuate four buttons through optocouplers: up, down,
M1, M2. The reset stays out of the circuit ([ADR-008](DECISIONS.md)). With
closed-loop height this reaches any height, not only the presets
([ADR-009](DECISIONS.md)).

**Ruled out: bus injection.** Electrically impossible,
[ADR-011](DECISIONS.md). The bus is read-only for good.

- [x] Verify the state of the actuation GPIOs at boot and reset **before**
      connecting them ([ADR-010](DECISIONS.md)) *(⚠️ unticked until 2026-09-02;
      done on 2026-08-20, and it is what `test_output_channels` exists for)*
- [x] M1 and M2 pulses bounded independently of the main loop *(the ESP32 task
      watchdog, [ADR-024](DECISIONS.md), implemented 2026-08-23)*
- [x] Abort movement on any incoherent height reading *(stale height, stall,
      reversed direction and maximum time, all in `superviseTravel`)*

### Timing, all measured

| | Duration | Source |
|---|---|---|
| Minimum for the chip to see the press | 160 ms | Two scan periods [datasheet] |
| **Chosen pulse width** | **800 ms** | [ADR-027](DECISIONS.md) |
| Long pulse, to start continuous travel | **2800 ms** | [ADR-028](DECISIONS.md) |
| Tap → continuous travel | 2.2 to 2.6 s | Measured 2026-08-06 |
| Tap → store preset | 3.0 s | Measured 2026-08-06 |

⚠️ **Corrected on 2026-09-02.** This table gave the chosen pulse width as
**300 ms**, the value from [ADR-023](DECISIONS.md), which
[ADR-027](DECISIONS.md) superseded on 2026-08-21 after measuring that **at
300 ms the desk does not move at all**. The firmware has emitted 800 ms taps
since then. The old figure survived here for twelve days.

### Channel map, verified 2026-08-22

| Channel | Pin | Button | Code |
|---|---|---|---|
| 1 | GPIO 27 | Up | `0x47` |
| 2 | GPIO 26 | Down | `0x57` |
| 3 | GPIO 25 | Preset 80 cm | `0x67` |
| 4 | GPIO 33 | Preset 117 cm | `0x6F` |

**All four match what [PROTOCOLO.md](PROTOCOLO.md) predicted** back on
2026-08-06. Capture:
[cuatro-canales-verificados](capturas/2026-08-22-cuatro-canales-verificados.log).

⚠️ **Load with `arduino-cli`, not with the IDE.** The Arduino IDE consistently
uploaded the wrong sketch:

```
CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
"$CLI" compile --fqbn esp32:esp32:esp32 firmware/<sketch>
"$CLI" upload -p /dev/cu.usbserial-0001 --fqbn esp32:esp32:esp32 firmware/<sketch>
```

**Check after touching the hardware:** `tools/verificar_canales.py` fires all
four and compares the code the box sees against the expected one. The preset
channels brake as soon as they reveal their code, so there are no long trips and
the desk stays where it was. **It requires `PULSE_MS = 300`** so that up and
down move nothing during the sweep.

⚠️ **Two rules that came out of phase 3, and they still apply:**

1. **Send `h` and check the sketch answers before every test**
   ([ADR-026](DECISIONS.md)). The serial port stopped receiving for no
   identified cause.
2. **If the desk moves and the capture says nothing happened, the suspect is the
   capture.** That is exactly what happened, and it cost an afternoon: the pulse
   loop was **empty**, so the firmware was blind for the 300 ms the key was
   held. The captures from that afternoon saying "the channel does not respond"
   are **marked as misleading**, not deleted.

⚠️ **Button wires go to pins 3 and 4 of the PC817, never to 1 and 2.** Pins 1
and 2 already carry the ESP32, its 330 Ω resistor and ground: they are the
internal LED. Pins 3 and 4 are the switch, the handset side. **That separation
is the isolation.**

## Phase 4, Home Assistant 🔶 in progress

Target: HA on Ultron (Raspberry Pi 5). **Full catalogue of what gets exposed,
covering state, usage, events, controls and diagnostics, in
[INTEGRACION_HA.md](INTEGRACION_HA.md)**, opened 2026-08-22.

Software height limits and safe-movement conditions belong here. See
[SEGURIDAD.md](SEGURIDAD.md).

### The risk that was blocking the phase, measured and dismissed

**The WiFi radio had never been switched on with this board.** The sniffer
samples at 4 MHz with interrupts off for 2 ms per burst, and the WiFi stack
needs CPU: they could have spoiled each other, and this board **had already hung
twice on 2026-08-03** from interrupt saturation.

**Measured on 2026-08-22, two 60 s runs identical except for the radio:**

| | No WiFi | With WiFi |
|---|---|---|
| Bursts | 299 | 298 |
| Transactions | 1502 | 1499 |
| **Malformed** | **0.67%** | **0.93%** |
| Late samples | 15 | 17 |
| Clock | 137 kHz | 137 kHz |

**The radio does not degrade the capture.** The difference falls inside the
already documented noise floor (~0.8%). Cost: the program goes from 21% to 67%
of flash and RAM from 9% to 16%. Capture:
[wifi-impacto](capturas/2026-08-22-wifi-impacto.log).

⚠️ **Assumed, not verified:** this is **AP mode with no clients**, the gentlest
case. **It still needs measuring in STA mode with real traffic** before calling
the architecture sound.

### Software presets, scope decided 2026-08-22

**Named heights, defined in software, independent of the handset's two
presets.** `{"standing": 117, "sitting": 75, "meeting": 95}`, with the height
reached by the height control that already works.

**Nothing new is needed.** It is demonstrated: in the full-travel test of
2026-08-22 the desk went to **95 cm, which is not one of the handset presets**.
Reading the height (phase 2) plus actuating (phase 3) plus braking on height
(tested) is all it takes.

**Advantage over the handset presets:** there are two of them, unnamed, and
changing one requires a 3 s press with the risk of overwriting what was there.
Software presets are unlimited and edited in a file. **And they cannot collide:**
the long pulse is bounded at 2800 ms ([ADR-028](DECISIONS.md)), below the 3.0 s
that store a preset, so **software cannot overwrite a handset preset even by
accident**.

**Limits, and they are real:**

- **1 cm resolution.** The display gives whole centimetres and about 1 cm of
  coasting remains after braking. It is anticipated and trimmed with taps, and
  all three targets on 2026-08-22 were hit exactly, but **below a centimetre
  there is no information**.
- **The ESP32 has to survive the trip.** A hang during continuous travel is
  stopped by nobody ([ADR-028](DECISIONS.md)). **Under supervision** until the
  software limits exist.
- **The display sleeps** on inactivity: it has to be woken with a tap before the
  first reading can be trusted.

### Work order

1. ⚠️ **Software height limits, first.** That is the condition
   [ADR-028](DECISIONS.md) sets for removing supervision from continuous travel.
   **It comes before any button reachable from a phone**, because today the
   brake depends on the ESP32 still being alive.
2. Repeat the WiFi measurement **in STA mode with traffic**.
3. ~~Choose a transport~~ → **closed 2026-08-22, MQTT with Discovery**
   ([ADR-030](DECISIONS.md)).
4. Controls and named presets.
5. Diagnostics and usage statistics.

## Phase 5, app ⬜

A custom frontend against the HA WebSocket API. JARVIS-style HUD aesthetic
(cyan, JetBrains Mono), consistent with Aitri Hub.

Metrics: hours sitting versus standing, posture change reminders, raising the
desk automatically when the first afternoon meeting starts.
