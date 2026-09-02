# Automated standing desk with an ESP32 and Home Assistant

This project automates a **Jiecang** standing desk (sold as Cougar) by reverse
engineering the bus that runs between its control box and the physical handset,
so the real height can be read at any moment and the desk can be driven from
Home Assistant.

None of it requires opening the control box or modifying the handset, and
manual use is never lost. The handset keeps working exactly as it did, and it
stays the panic button of the whole system.

> **Status: working.** The desk is read and controlled from Home Assistant
> through 19 entities, and the project is documented well enough that anyone can
> pick it up again by reading `docs/` alone.

> **On language.** Public facing documentation is in English. The engineering
> log ([BITACORA.md](docs/BITACORA.md)) and the decision records
> ([DECISIONS.md](docs/DECISIONS.md)) stay in Spanish on purpose: they are a
> working diary written while the project happened, and translating them
> afterwards would risk turning a recorded assumption into a stated fact, which
> is the exact failure this project documents against.

## What it does

It reads the height live by decoding the handset bus, and it moves the desk to
whatever height you ask for. Request 95 cm and it goes, braking on its own.
Everything reaches Home Assistant over MQTT: height, state, link health,
buttons. It also tells apart who moved the desk, so a press on the physical
handset is never confused with a command from the ESP32.

## How it works

The handset carries an **AiP650E**, a TM1650 clone that drives the display and
scans the keyboard. It talks to the control box over a two wire bus that looks
like I²C but has no addressing at all, running at roughly 202 kHz.

To read it, a resistive probe taps both lines into the ESP32, which samples at
4 MHz in bursts and decodes the frames. The bus is never written to.

To act on it, four **PC817** optocouplers sit in parallel with the handset
buttons as galvanically isolated dry contacts. The ESP32 presses buttons, and
the control box cannot tell those presses from a finger.

```
control box ──2-wire bus──> handset (AiP650E)
                   │              │
                 probe        4 buttons
                   │              │
                   └──> ESP32 <───PC817 x4
                          │
                       WiFi/MQTT ──> Home Assistant
```

## What has been measured

All of this was verified against the real hardware. None of it was inferred
from datasheets.

| | |
|---|---|
| Key codes | up `0x47`, down `0x57`, presets `0x67` and `0x6F` |
| Physical range | 73 to 118 cm |
| Speed | 0.68 cm/s, the same in both directions |
| Coasting after braking | ~1 cm |
| Minimum for a key to register | 160 ms |
| Tap to continuous travel | 2.2 to 2.6 s |
| Tap to **overwrite** a preset | 3.0 s |
| Idle bus health | ~0.7% malformed frames |

Those last two thresholds govern the whole design, because nothing reachable
from a phone may ever hold a contact closed that long.

## Safety

A desk that moves on its own can hurt someone, so the protections come in
layers. There are mechanical end stops, and the handset stays connected as a
physical stop. Galvanic isolation means no ESP32 voltage can reach the control
box. Every pulse width is bounded well clear of the dangerous thresholds, and a
watchdog reboots the chip if it ever hangs with a contact closed, which opens
that contact.

Travel is supervised: it brakes on target, on limit, on a stale reading, on a
stall, on reversed direction and on maximum time, and afterwards it verifies
that the brake actually took. Home Assistant watches from outside and alerts the
phone if anything moves for more than 3 minutes, if the link degrades, or if the
ESP32 disappears.

⚠ **Known limit.** If the ESP32 dies mid travel, nothing brakes it in software,
because braking requires *closing* a contact. What remains is the physical stop
and the handset.

⚠ **Known and not yet evaluated.** The long pulse holds a contact closed for
2800 ms and cannot be aborted. Press the opposite key during that window and the
control box sees UP and DOWN at once. There is no electrical risk, but what the
box does with that combination has not been verified. See
[SEGURIDAD.md](docs/SEGURIDAD.md).

## Documentation

The project follows an explicit discipline, written down in
[POLITICA_DOCUMENTACION.md](docs/POLITICA_DOCUMENTACION.md). Every technical
claim is marked as verified, assumed or ruled out. Irreversible decisions become
ADRs. Raw captures are kept unedited. And there are no silent corrections: when
something turns out to be wrong it gets fixed *and* the fact that it was wrong
gets recorded.

| Document | What it holds | Language |
|---|---|---|
| [PLAN.md](docs/PLAN.md) | Phases and the concrete next step | English |
| [PROTOCOLO.md](docs/PROTOCOLO.md) | The bus, decoded | English |
| [HARDWARE.md](docs/HARDWARE.md) | Measurements and wiring | English |
| [SEGURIDAD.md](docs/SEGURIDAD.md) | Risks and rules | English |
| [INTEGRACION_HA.md](docs/INTEGRACION_HA.md) | What is exposed to Home Assistant | English |
| [hardware/PCB_ESPECIFICACION.md](docs/hardware/PCB_ESPECIFICACION.md) | Board specification and schematics | English |
| [capturas/](docs/capturas/) | Raw bus dumps, with their context | English |
| [DECISIONS.md](docs/DECISIONS.md) | 34 ADRs with their reasoning | Spanish |
| [BITACORA.md](docs/BITACORA.md) | Session diary, mistakes included | Spanish |
| [POLITICA_DOCUMENTACION.md](docs/POLITICA_DOCUMENTACION.md) | The documentation rules themselves | Spanish |
| [COMPRAS.md](docs/COMPRAS.md) · [REFERENCIAS.md](docs/REFERENCIAS.md) | Shopping list and sources | Spanish |

The log keeps the wrong turns on purpose. A handset written off as burnt that
turned out to be a solder bridge. A channel reported dead that was a bug in the
sniffer itself. Two rounds of adversarial review that found regressions
introduced by the previous round of fixes.

## Building it

You need an ESP32 DevKit, four PC817 optocouplers, a handful of resistors, and a
Jiecang desk with a `JK-CH506` handset or something compatible.

1. Copy `firmware/desk_sniffer/secrets.h.example` to `secrets.h` and fill in your credentials
2. Compile and flash with `arduino-cli`, described in [firmware/README.md](firmware/README.md)
3. Point it at a reachable MQTT broker. The entities show up on their own through discovery

⚠ **Read [SEGURIDAD.md](docs/SEGURIDAD.md) before touching hardware.** The
connection order matters: USB first, bus wires second.

## License

MIT, see [LICENSE](LICENSE).

The datasheet under `docs/hardware/datasheets/` belongs to the manufacturer and
is included here because the original is no longer downloadable. Its rights
belong to I-CORE.
