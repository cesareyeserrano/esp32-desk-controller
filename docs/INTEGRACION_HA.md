# Home Assistant integration

> What the desk exposes to HA and why. Phase 4 design, opened on 2026-08-22.
> Firm decisions live in [DECISIONS.md](DECISIONS.md); this is the catalogue and
> the reasoning.
>
> *Documento en inglés desde el 2026-09-02. La bitácora y los ADR siguen en
> español; ver [POLITICA_DOCUMENTACION.md](POLITICA_DOCUMENTACION.md).*

Target: **HA on Ultron (Raspberry Pi 5)**.

---

## The principle everything else follows from

**The ESP32 publishes facts. Home Assistant derives statistics.**

The ESP32 publishes what it *observes*, meaning height, keys and bus health, and
HA takes care of accumulating, storing and graphing. The reasons:

- **HA already has a database, graphs and long-term statistics.**
  Reimplementing that on a microcontroller is wasted work.
- **Anything the ESP32 accumulates is lost on reboot.** "Standing time today"
  held in RAM goes back to zero with every USB interruption.
- **The firmware stays small**, and small firmware is the firmware that keeps
  working. The sniffer is timing-critical code; the less it carries, the better.

**Exception:** counters that need sampling faster than the publish rate, such as
total distance travelled which changes at 0.68 cm/s, are accumulated on the
ESP32 and published already summed.

---

## What already works, 2026-08-22

**Thirteen entities, created automatically through discovery.** Verified end to
end: Home Assistant → MQTT → ESP32 → optocoupler → handset → control box, with
the confirmation coming back over the bus.

| Type | Entities |
|---|---|
| Sensors | height, height age, bus malformed, bus transactions, uptime, WiFi RSSI |
| Binary | display awake, **online** (MQTT last will: the broker sets it `off` if the ESP32 dies, without depending on the firmware) |
| Buttons | up, down, preset 1, preset 2, **stop**, refresh height |

⚠️ **Only taps are exposed, never the long pulse.** Nothing pressable from a
phone can start continuous travel (2.2 s) or overwrite a preset (3.0 s). That is
a decision, not an omission: continuous travel still needs supervision
([ADR-028](DECISIONS.md)).

⚠️ **What can happen from the phone: pressing M1 or M2 starts a trip of up to
44 cm**, because the control box runs it on its own. The **stop** button is on
the same screen precisely for that.

Capture: [controles-mqtt](capturas/2026-08-22-controles-mqtt.log).

**Missing:** the `number` for going to a specific height, named presets and
usage statistics, and before any of that, **the software limits**.

**Since 2026-08-23 the ESP32 runs from a wall charger**, independent of the Mac.
The serial port is unavailable unless it is reconnected to the Mac. Monitoring
and commands go over MQTT.

⚠️ **When changing the ESP32's power supply**, the usual order: desk unplugged →
swap the USB → wait for boot → desk back to mains. The instant without power
while the wires are attached is the condition of
[ADR-019](DECISIONS.md)/[ADR-031](DECISIONS.md).

### Automatic monitoring, installed 2026-08-23

Five automations in HA (in `automations.yaml`, editable from the interface),
notifying the phone **iCesar pro**:

| Automation | When it warns |
|---|---|
| ALERT disconnected | The ESP32 has not published for 2 min (detected by **the broker**, not the firmware) |
| Back online | On reconnection |
| ALERT bus degraded | Malformed >2% sustained for 10 min, meaning the probe is degrading |
| 🚨 Sustained movement | Rising or falling >3 min. No legitimate trip lasts that long (the full range is ~65 s) |
| Summary every 30 min | "Desk OK: height, bus, WiFi, movement". **Only if online**; if it gets tiring, it is disabled from the interface and only the alerts remain |

**The logs already keep themselves**: the HA recorder holds the history of every
entity (30 days as configured), viewable in the history panel. For long-term
study there is the InfluxDB integration already present, currently configured
with no entities included; if a permanent desk history is wanted, they get added
there.

**Note**: `altura` publishes `unknown` when the display sleeps; the discovery
config carries a `value_template` so HA treats it as an unknown state rather
than an error.

## Desk state

What is needed to know what is happening right now.

| Entity | Type | Source | Status |
|---|---|---|---|
| `altura` | sensor, cm | Bus display | ✅ **Already measured** |
| `estado` | sensor: `quieto`/`subiendo`/`bajando` | Height evolution + key seen | 🔶 Derive |
| `altura_objetivo` | sensor, cm | From the controller, while travelling | 🔶 Implement |
| `display_despierto` | binary_sensor | All 4 digits at `0x00` = asleep | ✅ **Already measured** |
| `antiguedad_altura` | sensor, s | Seconds since the last valid reading | 🔶 Derive |

⚠️ **`antiguedad_altura` is not diagnostic decoration: it is safety.**
[ADR-012](DECISIONS.md) says no movement starts on a stale height, and this is
the entity that makes that checkable from HA. **If the display has been asleep
for a while, the height shown is the last known one, not the current one.**

---

## Usage: what makes the integration interesting

This is where it stops being a remote control and becomes something that knows
things.

| Entity | Type | How it comes about |
|---|---|---|
| `tiempo_de_pie_hoy` | sensor, min | HA accumulates over `altura` and a threshold |
| `tiempo_sentado_hoy` | sensor, min | Same |
| `cambios_de_altura_hoy` | counter | HA, over movement events |
| `altura_min_hoy` / `altura_max_hoy` | sensor, cm | HA, daily statistics |
| `ultimo_movimiento` | timestamp | HA |
| `distancia_recorrida_total` | sensor, m | **ESP32**, accumulating \|Δheight\| |
| `pulsaciones_por_boton` | 4 counters | ESP32 or HA, over key events |

**The standing/sitting threshold has to be chosen**, and there is no universal
value: it depends on height. The physical range is 73 to 118 cm. Left as a
[pending decision](#pending-decisions).

**`distancia_recorrida_total` is the motor wear indicator.** It is the one
number that will say something when the desk starts failing years from now.

---

## Events: where most of the value is

**The sniffer sees the physical handset keys, not only the ones the ESP32
sends.** That means HA can find out that **a person has touched the handset**,
which opens up automations that otherwise would not exist.

| Event | When | What it is for |
|---|---|---|
| `boton_pulsado` | Any key on the bus, with its code | Telling a person from an automation |
| **`uso_manual`** ✅ | Seconds since the last human key on the handset | **Implemented 2026-08-23.** The automation that does not want to fight the person checks this |
| `movimiento_no_pedido` ✅ | The desk moves without the ESP32 asking | **Implemented 2026-08-23 as automatic give-way**: a key the ESP32 did not order cancels its trip without braking (the person already braked). Verified live: `ultimo_freno: mando manual` |
| `preset_recuperado` | `0x67` or `0x6F` on the bus | Knowing the destination before arrival |
| `tope_alcanzado` | The height stops changing at 73 or 118 | End of travel |

**Verified**: all four key codes are read off the bus and told apart
unambiguously, `0x47` up, `0x57` down, `0x67` M1/80 cm, `0x6F` M2/117 cm, and
the sniffer sees them **whether the ESP32 or a person caused them**.

## Boot and state recovery

**Measured on 2026-08-22, including cutting the power mid-travel:** it keeps the
height, keeps the presets, **does not resume movement** when power returns, and
the bus revives on its own. What does not survive is the ESP32's knowledge: the
display **starts off**, and with the display off **the height is not on the
bus**.

**Refreshing costs nothing:** thirteen 300 ms taps measured, zero drift. It can
be used as often as needed.

**Boot sequence, and the order matters:**

1. Publish the last known height, **marked as unconfirmed**, with
   `antiguedad_altura` high. This requires storing it in the ESP32 flash (NVS),
   not just RAM.
2. Give the **refresh tap** (`w`, 300 ms): it wakes the display **without moving
   the desk**, and both halves of that are verified.
3. Publish the real height and set `antiguedad_altura` to zero.

⚠️ **Step 1 cannot be skipped, and neither can step 3.** The ugly case is this:
power returns, HA shows 95 cm because that is the last thing it saw, and it
turns out somebody moved the desk by hand while the power was out. **An old
height presented as current is worse than no height at all.**

⚠️ **Never refresh through a preset channel.** Any tap on M1 or M2 starts a trip
to the preset. The `w` command uses the down channel for that reason.

## The reset button: what we do not know

The handset has a reset button that is **deliberately not wired**
([ADR-008](DECISIONS.md)): it drives down to the bottom stop, with no
confirmation and no way to interrupt.

**Not being wired does not mean a person cannot press it.** And there are two
unknowns there that affect the integration:

- **The ESP32 would see it** as movement it did not request, which the
  `movimiento_no_pedido` event in this catalogue covers.
- ⚠️ **It is not known whether the reset alters the presets.** If it
  recalibrates the zero, the heights of M1 and M2 could stop meaning what they
  meant. **Assumed, not verified**, and checking it costs a full run of the
  desk.

  **Hence [ADR-029](DECISIONS.md): the system associates no height with M1 and
  M2.** If it never claims M1 means 80, it cannot lie when that stops being
  true. Software presets **do not depend on the handset's memories**, so they
  would not be displaced either.

**Until it is checked:** if HA detects a long unrequested descent ending at the
bottom stop, the prudent move is to **mark the height as unreliable** and ask
for confirmation before using presets again.

## Controls

| Entity | Type | Notes |
|---|---|---|
| `ir_a_altura` | number, 73 to 118 | Closed loop, tested on 2026-08-22 |
| `subir` / `bajar` | button | 800 ms tap ([ADR-027](DECISIONS.md)) |
| `parar` | button | A tap on any channel: **it is the brake** |
| `preset` | select | Named heights, in software |
| `permitir_movimiento` | switch | Master lock |
| `M1` / `M2` | button | **Opaque** buttons: no associated height ([ADR-029](DECISIONS.md)) |
| `m1_altura_observada` | sensor + date | Where it went **last time**. An observation, not configuration |

⚠️ **`parar` is not optional.** With continuous travel, a tap is the only thing
that stops the desk ([ADR-028](DECISIONS.md)). It has to be on any interface
that can start a trip.

---

## Diagnostics: link health

Without this, when something fails three months from now there will be no way to
tell whether it is the bus, the radio or the firmware. And this session has
already shown how expensive it is not to be able to tell them apart.

| Entity | Source | Available |
|---|---|---|
| `bus_transacciones_s` | Sniffer statistics | ✅ |
| `bus_malformadas_pct` | Same, **the capture health indicator** | ✅ |
| `bus_reloj_khz` | Same | ✅ |
| `muestras_tardias` | Same | ✅ |
| `wifi_rssi` | ESP32 | 🔶 |
| `uptime` / `motivo_ultimo_reinicio` | ESP32 | 🔶 |
| `memoria_libre` | ESP32 | 🔶 |

**Reference measured on 2026-08-22, bus idle, to know what normal looks like:**
299 bursts and 1502 transactions per minute, **0.67% malformed**, 137 kHz. With
the radio on: 0.93%. See
[capturas/2026-08-22-wifi-impacto.log](capturas/2026-08-22-wifi-impacto.log).

---

## Posture reminders, installed 2026-08-23

**They only warn if you are there.** Presence sensor `SNZB-06P` (mmWave: it
detects you even when still, not only movement).

| Automation | Fires on | Sequence |
|---|---|---|
| Sitting too long | **45 min** seated, **presence**, and **no handset use in 5 min** | Warns → waits 110 s → **re-checks presence** → raises to 117 → button *"Leave it at 80"* |
| Standing too long | **30 min** standing, same conditions | Same, towards 80 |

⚠️ **Two faults that stopped them firing, corrected on 2026-08-24**, spotted by
the owner when they never triggered:

**1. The trigger waited for a change that never happens.** It was written as
*"when posture CHANGES to seated and stays 45 min"*. If the desk was already at
80 from the previous day, the posture **already was** `sentado`: there is no
transition to catch and it never fires. Now it checks **every 5 min how long it
has been** in that posture, which is what was meant to be measured all along.

**2. A condition against a sensor that did not exist.** The courtesy rule *"do
not move if the handset was touched in the last 5 min"* used `uso_manual`,
**which the firmware only published after somebody touched the handset for the
first time**. With it never touched, the sensor does not exist, the condition
cannot be evaluated, and **it blocked the whole automation silently**. Now, if
the sensor is missing it is read for what it means, that the handset was never
used, and it lets through.

**The second one is the treacherous pattern**: a safety condition that, being
unevaluable, prevents operation rather than allowing it. And it leaves no trace
in the log: the automation simply does not fire.

**Verified on 2026-08-24** by dropping the threshold to 60 s and leaving only
the notification: it arrived on the phone.

### The time counters: how they are computed and what went wrong

**Full chain**, bottom up:

```
height (from bus)  →  stable height  →  posture  →  effective posture  →  daily counters
                      (keeps the        (only at    (only if somebody
                       last real one)    targets)    is present)
```

| Link | What it solves |
|---|---|
| `altura_estable` | The height disappears (`unknown`) when the display sleeps |
| `postura` | Translates height into posture, **only at the working heights** |
| `presencia_sostenida` | Presence with a 15 min `delay_off`: a short trip away does not count against you |
| `postura_efectiva` | `ausente` if nobody is there: **a desk left up overnight does not rack up "standing" hours** |
| `history_stats` | Standing / sitting / count, today and over 7 days |

⚠️ **Fault corrected on 2026-08-28, pointed out by the owner:** *"if I pause a
raise or lower, it still counts as if it had gone up"*.

Posture used **a single threshold at 95 cm**, with two consequences:

1. **It changed mid-trip.** Rising from 80 to 117, crossing 95 already counted as
   "standing", thirty seconds before arriving
2. **An interrupted trip lied.** Stopping at 96 left the posture as "standing"
   indefinitely, and **the daily counters added up those hours**

**Posture now only exists at the working heights:**

| Height | Posture |
|---|---|
| 77 to 83 (target **80**) | `sentado` |
| 114 to 120 (target **117**) | `de pie` |
| anything else | **`intermedia`**, counts for nothing |

The ±3 cm covers manual adjustments. **If you are not in a working posture,
there is nothing to time**, which is exactly the owner's proposal and removes
the problem at the root instead of patching it.

### The `movimiento` sensor tells who is moving the desk

| Value | Meaning |
|---|---|
| `quieto` | Stopped |
| `subiendo` / `bajando` | **The system** is running a trip |
| `frenando` | Brake issued, verifying that it stops |
| `subiendo (mando)` / `bajando (mando)` | **A person** is moving it with the handset |

The `(mando)` states **come from no order at all**: they are inferred by watching
the height change on the bus while the ESP32 is idle. If the height rises and
nobody asked for it, somebody has a finger on the button. It returns to `quieto`
after 4 s without changes, since the desk reports its height every ~1.5 s while
travelling.

**It matters beyond information**: the automation that stops the desk if you
disappear while it moves consults this sensor, so it now also covers **continuous
travel you started by hand and left running**.

### One sensor cannot answer two different questions

⚠️ **Real failure on 2026-09-03: the desk rose with nobody in the room.** The
owner reported it, and the records confirmed it exactly:

```
13:13:58  raw presence        off          <- he is gone
13:16:00  raw presence        unavailable  <- the sensor is not even answering
13:16:56  MOVEMENT            rising       <- the desk goes up anyway
13:17:40  HEIGHT              116
13:28:58  sustained presence  off          <- twelve minutes late
```

**This was caused by the fix of the previous day.** On 2026-09-02 the
pre-movement re-check was moved from the raw sensor to `presencia_sostenida`,
because the raw one flickers and was cancelling movements with the owner sitting
right there. That fixed the cancellations and **opened this**: the sustained
sensor carries a 15 minute `delay_off`, so it kept saying "yes" for a quarter of
an hour after he left.

**It is worse than the fault it replaced.** [SEGURIDAD.md](SEGURIDAD.md) requires
confirmation that somebody is in front of the desk, and this accepted a
confirmation that was fifteen minutes stale.

**The mistake was using one sensor for two different questions:**

| Question | Tolerance | Sensor |
|---|---|---|
| *Is it still their turn to be sitting?* | 15 min. A trip to the bathroom is not a posture change | `escritorio_presencia_sostenida` |
| *May I move a piece of furniture right now?* | **3 min**, and not one more | **`escritorio_presencia_reciente`** ← new |

Three minutes still absorbs the measured mmWave flicker, which drops and returns
every 1 to 2 minutes, without ever authorising a movement in front of an empty
chair.

⚠️ **And a second hole, from the same event:** the sensor went to `unavailable`,
not to `off`. The watch that stops the desk mid-travel triggered only on `off`,
so **a sensor that had fallen off the network read as "nothing to react to"**.
It now triggers on `unavailable` and `unknown` too. A sensor that does not answer
is not evidence that somebody is there.

#### And the same day, the opposite failure: it braked a legitimate trip

Two hours after the fix above, the owner pressed preset 1 in HA and **the desk
stopped at 111 cm instead of reaching 80**.

```
14:18:30  MEMORY 1 pressed  -> the box starts descending from 117 towards 80
14:18:38  raw presence      -> off, for TWO seconds
14:18:39  the stop-on-absence automation fires and publishes `parar`
14:18:42  the desk stops at 111
14:18:40  the sensor is back to on, too late
```

He was sitting there the whole time. **A two-second flicker aborted a legitimate
trip**, and pressing preset 1 again 45 seconds later completed the full 29 cm
without trouble.

**And the fix from that same morning made it worse:** adding `unavailable` and
`unknown` to the watch made it more sensitive, so the layer that stops the desk
when you leave started stopping it while you were there.

**Fix: a 10 second confirmation (`for`) on the trigger.** The layer still watches
the raw sensor, because here reaction speed is the point, but it no longer
believes a two-second blink.

**The cost, stated plainly:** if the absence is real, braking now happens 10 s
later, which at 0.68 cm/s is about **7 cm of extra travel**. A full trip lasts
~65 s, so it still brakes well inside the journey, and the physical stop and the
handset are still there underneath.

#### The three layers as they stand

| Layer | Question it answers | Sensor | Tolerance |
|---|---|---|---|
| 1. Trigger | Is it their turn to change posture? | `presencia_sostenida` | 15 min |
| 2. Re-check before moving | May I move furniture right now? | `presencia_reciente` | 3 min |
| 3. Stop mid-travel | Did they leave *while it moves*? | raw sensor | **10 s** |

**Three questions, three tolerances, three sensors.** Every failure in this area
came from making one sensor answer two of them.

### The counter measures YOUR posture, not the desk's height

Raised by the owner as a use case: *"I go to the bathroom and I'm not there, so
it doesn't rise. When I come back, does it count from zero or does it understand
I just got back? And if I come back in an hour?"*

**The initial design got this wrong.** The counter was *"how long the desk has
been at this height"*, so being away for an hour changed nothing: on your return
it would say "you have been sitting for an hour and a half" and raise the desk
**just as you had sat down**. It was measuring the furniture, not the person.

**There is now an `input_datetime` marking the start of the posture period**, and
two things move it:

| Situation | What happens |
|---|---|
| **You change posture** | New period. Obvious |
| **Absence < 15 min** (bathroom, coffee) | **It is left alone.** You did not really lose the posture |
| **Absence ≥ 15 min** (lunch, a meeting) | **Resets** and tells you: you were on your feet, that counts as a break |

The 15 minute threshold separates an errand from a real break. It is changed in
the `escritorio_periodo_vuelta_larga` automation.

### Three layers against "they left right then"

The sensor does not know instantly that you have gone, so **no single check is
enough**. The owner pointed this out twice, *"it might think I'm still here"*
and, once the re-check was added, *"doesn't matter, I can leave right at the
confirmation"*. Right both times: a check narrows the window, it does not close
it.

| Layer | What it does |
|---|---|
| **1. Sustained presence** (`delay_off: 15 min`) | The real layer. ⚠️ **Rewritten 2026-09-02:** this layer used to rely on the sensor's own 120 s delay, and **the device reverts it to 30 s on its own**. Worse, the raw sensor **flickers every 1 to 2 minutes** with the person sitting right there (measured, see below). Only the sustained sensor, which lives in Home Assistant, survives that flicker |
| **2. Re-check after 110 s** | Warns, waits, and **asks again** before moving, against `presencia_reciente` (3 min). ⚠️ **Rewritten twice.** It asked the **raw** sensor until 2026-09-02, cancelling movements with the owner sitting right there. The fix pointed it at the **sustained** sensor, and that broke it the other way: on 2026-09-03 the desk rose with nobody there, see below |
| **3. Stop if you leave WHILE it moves** | Continuous watch: if presence drops with the desk in motion, `parar` is sent and you are told. **It depends on the `movimiento` sensor**, which was inoperative until 2026-08-24 because that sensor did not update during travel. Watches the **raw** sensor with a **10 s confirmation** (`for`), and triggers on `off`, `unavailable` and `unknown` |

**Layer 3 is the one that closes the case** the other two cannot: it does not
predict, it reacts.

⚠️ **Correction of a mistake of mine:** I said that lowering the sensor delay
would break the posture counter. **That is false.** The counter measures HEIGHT
(`sensor.escritorio_postura`), not presence; presence is only consulted at the
moment of firing. Lowering it breaks nothing and improves everything else.

**They move the desk.** The first version only notified with a button, and the
owner pointed out the obvious: *"if it only warns you to move, what is the ESP32
for?"* Nothing; an alarm clock does that.

**And it satisfies [SEGURIDAD.md](SEGURIDAD.md)**, which asks for *"confirmation
that somebody is in front of it"*: **the presence sensor IS that
confirmation**. Requiring a button press on top was one precaution too many, and
it emptied the system of its usefulness.

Three conditions, and the third is courtesy: **it does not move if you touched
the handset in the last 5 minutes** (`uso_manual`). If you just put it where you
want it, it does not change it on you.

### The derived sensor that turned out to be necessary

`sensor.escritorio_jiecang_altura` publishes **`unknown` when the display
sleeps**, because with the display off the height is not on the bus. Measuring
"how long have I been in this posture" with it is impossible: the value
disappears every few minutes.

Hence two derived sensors in `configuration.yaml`:

- **`sensor.escritorio_altura_estable`**, which keeps the last real height
- **`sensor.escritorio_postura`**, which gives `sentado` / `de pie` /
  `intermedia`

⚠️ **Corrected on 2026-09-02.** This paragraph used to say posture worked off a
**single threshold at 95 cm** and that 95 was "the number to change if it does
not match your posture". **That stopped being true on 2026-08-28**, when the
single threshold was replaced by the two working ranges described above, exactly
because a single threshold flipped mid-trip and lied about interrupted trips.
The old text survived four days two sections below its own correction, which is
what happens when a fix is written in one place and the document is not re-read
whole.

⚠️ **And it is not refreshed with periodic taps.** Having the summary wake the
display every 30 min was tried: that is button wear and a lit screen to read a
number that hardly ever changes. **It is not needed**: when somebody moves the
desk, the display comes on by itself and the sensor updates.

**The working heights are 80 (sitting) and 117 (standing)**, given by the owner.

⚠️ **A trap that cost a while:** a template trigger written in the new syntax
(`- trigger: state`) inside the old key (`trigger:`) **passes HA validation and
never fires**. The sensors sit at `unknown` without a single error in the log.
It has to be `- platform: state`.

### The presence sensor flickers: measured, not assumed

**The owner said so twice before I checked:** *"but I was there, I haven't moved
from here"* and *"not possible, I've been moving a lot, it isn't the sensor"*.
Both times I answered by blaming him for sitting still. **Both times I was the
one who was wrong.**

Querying the `recorder` database settled it. Six hours of 2026-09-02, same
person, same place:

| Entity | Changes in 6 h |
|---|---|
| `binary_sensor.…_presencia` (raw) | **dozens**, dropping and returning every 1 to 2 min |
| `binary_sensor.escritorio_presencia_sostenida` | **4** |

The exact window of the 11:25 failure, straight out of the database:

```
11:24:21 off   11:24:40 on   11:25:24 off   11:27:02 on   11:32:23 off …
```

The re-check landed in the gap at `11:25:24`. **This was not a one-off piece of
bad luck: with that flicker, any instantaneous check is a coin toss.** Hence
both the trigger and the pre-movement re-check now use `presencia_sostenida`,
never the raw sensor.

#### It is not the Zigbee link: measured 2026-09-02

The owner moved the sensor to a USB port on the Pi and asked whether that was a
problem. I answered that it was, on three counts: USB 3.0 noise at 2.4 GHz
along the power cable, the magnet sticking the sensor to a metal rack, and the
coordinator supposedly being inside that same rack.

**He stopped me: I was conflating the presence sensor with the Zigbee
coordinator**, and he was right. Where the coordinator sits was never
established. I had assumed it and written the assumption as fact.

**Measured instead of argued**, from `zigbee.db` (`neighbors_v15.lqi`, 0 to 255):

| Who sees the sensor | LQI |
|---|---|
| Nearby router | **200** |
| **The coordinator** (`00:12:4b…`, nwk=0) | **199** |
| Another router | 121 |
| Another | 112 |

**199 direct to the coordinator is excellent**, and better than most other
devices on this network, which sit between 110 and 170.

⚠️ **Hypothesis ruled out.** Neither the metal rack nor the USB 3.0 port is
degrading the radio. The message arrives fine. **Do not relocate the sensor**:
it would be work for nothing.

**What it leaves standing:** the flicker is in the **mmWave detection**, not in
the transmission. The sensor decides you have left and reports that decision
without trouble. The fault is in the decision, not in the delivery.

**How to re-check** (open read-only, the coordinator holds the file):

```bash
docker exec homeassistant python3 -c "
import sqlite3
z = sqlite3.connect('file:/config/zigbee.db?mode=ro', uri=True)
for dev, nei, lqi in z.execute(
        'SELECT device_ieee, ieee, lqi FROM neighbors_v15 ORDER BY lqi DESC'):
    print(nei, lqi, 'seen by', dev)
"
```

#### The sensor delay has been at 30 s since 31 August

For a while I believed the Zigbee device *reverted its delay on its own* and
recorded it as a mystery. **The records say otherwise**, and most of it is not
mysterious at all:

| Date and time | Delay | Automation that corrects it |
|---|---|---|
| 2026-08-31 10:03 → 10:21 | **120** | **on** |
| 2026-08-31 20:58 | back to **30** | on |
| 2026-08-31 21:03 | | **off** |
| since then | **30**, always | off |

The reason it **stays** at 30 is not the device: it is that
`automation.escritorio_retardo_del_sensor_de_presencia_a_30_s`, which sets 120
when Home Assistant starts, **has been disabled since 2026-08-31 at 21:03**. It
was turned off and nobody turned it back on.

⚠️ **Its alias lies:** it says *"a 30 s"* while its code sets **120**. That is
the old name, from before [the correction of 2026-08-31](BITACORA.md). A name
that says the opposite of its code is exactly the kind of thing that costs an
afternoon.

**What does remain unexplained**, and it is only one row of the table: why the
value went from 120 to 30 at 20:58 that day with the automation still enabled.

#### 30 or 120: what the records say, and what the owner says

I was about to re-enable the 120 until the owner stopped me: *"last week the
sensor worked fine with the configuration we had, the day before yesterday we
set it to 120 and it started working badly"*. **Nothing was touched: it stays at
30.**

Measured over the `recorder`, state changes of the **raw** sensor during working
hours (09:00 to 19:00):

| Day | Delay | Changes/h | Dropouts under 3 min |
|---|---|---|---|
| Wed 26 Aug | 30 | 9.1 | 37 |
| Thu 27 Aug | 30 | 12.0 | 53 |
| Fri 28 Aug | 30 | 4.7 | 17 |
| **Mon 31 Aug** | **120** | **2.1** | **5** |
| Tue 1 Sep | 30 | 19.6 | 84 |
| Wed 2 Sep | 30 | 8.1 | 33 |

**Both facts are true and they contradict each other less than it looks.** The
120 clearly reduces the flicker, giving the best of the six days, but *less
flicker is not the same as "works well"*: at 120 s the sensor takes two minutes
to admit an absence, and the pre-movement re-check happens at 110 s. **With 120,
that re-check can say "still there" about somebody who has already left**, and
the desk moves on its own. That is a reasonable hypothesis for what the owner
saw, and **it is unverified**.

⚠️ **What is measured:** the delay caused **neither** of the two faults of
2026-09-02. The counter being wiped on every restart and the re-check against
the raw sensor happen exactly the same at 30 as at 120.

**Decision: it is not touched without a measurement of the real symptom**,
meaning reminders that do not end in movement, rather than the number of
transitions, which is what I know how to measure but is not what bothers anyone.

Either way the operational conclusion does not change: **the device delay is not
a layer to lean on.** The real protection is `presencia_sostenida`, which lives
in Home Assistant and does not depend on the gadget.

### A Home Assistant restart is not standing up

**Fault found on 2026-09-02**, with the desk refusing to rise: *"it won't rise
because it says I've been sitting 5 min"*.

`automation.escritorio_reiniciar_periodo_al_cambiar_de_postura` fired on **any**
change of `sensor.escritorio_postura`. When Home Assistant starts, that sensor
goes from `unknown` to `sentado`, which is not a change of posture but the
sensor being born. The automation counted it as a new posture and **set the
counter to zero**. Effect: *every Home Assistant restart wiped the accumulated
hours*, and the reminders started over without anybody leaving their chair.

The condition now requires the **origin posture to be a real posture too**:

```jinja
{{ trigger.from_state is not none
   and trigger.from_state.state in ['sentado','de pie','intermedia']
   and trigger.to_state.state in ['sentado','de pie']
   and trigger.from_state.state != trigger.to_state.state }}
```

Verified by restarting Home Assistant on purpose: the counter **kept** its
value.

### The logs: look instead of guessing

Requested by the owner, *"I don't know if we have logs of the whole system, so
we're not going in blind on this stuff"*, after a whole session of plausible and
wrong diagnoses of mine.

**They do exist**, and they are the only source that settled any of this
session's arguments. Home Assistant keeps 30 days in SQLite (`recorder`,
`purge_keep_days: 30`).

```bash
ssh <user>@<HA-host> 'docker exec homeassistant python3 -c "
import sqlite3
con = sqlite3.connect(\"file:/config/home-assistant_v2.db?mode=ro\", uri=True)
for st, t in con.execute(
    \"\"\"SELECT s.state, datetime(s.last_updated_ts,\"unixepoch\",\"localtime\")
         FROM states s JOIN states_meta m ON s.metadata_id = m.metadata_id
         WHERE m.entity_id = ? AND s.last_updated_ts > strftime(\"%s\",\"now\",\"-8 hours\")
         ORDER BY s.last_updated_ts\"\"\", (\"sensor.escritorio_postura\",)):
    print(t, st)
"'
```

⚠️ **`last_triggered` is no use for knowing whether a reminder worked.** That
attribute reaches the database late: on 2026-09-02 it read `None` with the desk
already rising on that same automation's order. **The good source is the
movement**, meaning `sensor.escritorio_jiecang_movimiento` and the height, which
additionally tells apart the ESP32 (`subiendo`) from a person
(`subiendo (mando)`).

Three more things that save time:

- **Always open with `mode=ro`.** Home Assistant holds the database open;
  writing to it from outside corrupts it.
- **`entity_id`s cannot be guessed.** In this session I invented six plausible
  names and all six were wrong, which produced an entire report of imaginary
  "BLOCKED" verdicts. Get them first with
  `WHERE m.entity_id LIKE "%escritorio%"`.
- **The interface's `last_changed` lies after a restart**; the `states` table
  does not. For "how long have I been sitting", the good source is
  `input_datetime.escritorio_inicio_postura`.

## Dashboard

In the **Estudio** section of the `Casa` dashboard: the **Ir a altura** field
(cm) and the **movement indicator**. Only that, on purpose. The rest of the
entities live on the device page, without cluttering the daily dashboard.

## Pending decisions

None can be closed without information that is not in the repository.

1. ~~**Transport: MQTT or ESPHome.**~~ **Closed on 2026-08-22:
   [ADR-030](DECISIONS.md) fixes MQTT.** The deciding reason is that the sniffer
   blocks for up to 2.8 s per pulse and ESPHome requires components that return
   promptly: porting it would mean rewriting the critical code.

   **Broker already running**, brought up and verified that same day:
   `ultron:1883`, user `esp32`, password on Ultron at
   `~/mosquitto/config/.pass-esp32`.
2. **Standing/sitting threshold**, in cm. Depends on the person's height.
3. **WiFi credentials**, to repeat the impact measurement **in STA mode with
   real traffic**. What has been measured so far is AP with no clients, the
   gentlest case.
4. **The software limits**, which is the condition [ADR-028](DECISIONS.md) sets
   for removing supervision from continuous travel. **This comes before any
   button reachable from a phone.**
