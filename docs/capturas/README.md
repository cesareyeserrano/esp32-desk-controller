# Captures

Raw sniffer dumps. **They are never edited.** Not trimmed, not cleaned, not
reordered. An edited capture stops being evidence.

Interpretation belongs in [../PROTOCOLO.md](../PROTOCOLO.md), citing the file
and the line.

## File naming

```
YYYY-MM-DD-short-description.log
```

Examples: `2026-08-03-reposo-altura-74.log`, `2026-08-03-pulsador-subir.log`.

## Mandatory header

Every capture starts with a context block. A capture without context is a
useless file of numbers: a month later nobody remembers what height the desk was
at or which button was pressed.

*Corrected on 2026-08-03: the example below used to read `sonda 10k/20k en P16
(amarillo)`. Both things were wrong. That probe was never adopted, and yellow is
the 5 V wire, which is never connected. A template that gets copied cannot carry
the wrong wiring.*

```
# Fecha: 2026-08-03 18:40
# Altura en pantalla: 74.5
# Estado: escritorio quieto, nadie tocando el mando
# Montaje: sonda 9.1k/27k en P18 (rojo, CLK) y P4 (verde, DIO), ESP32 por USB
# Firmware: sniffer v1
# Qué se esperaba: tráfico periódico de refresco de display
```

## How they are produced

**Close the Arduino IDE Serial Monitor first.** Only one program can hold the
port open at a time.

```
exec 3</dev/cu.usbserial-0001
stty -f /dev/cu.usbserial-0001 460800 raw -echo
cat <&3 >> docs/capturas/YYYY-MM-DD-description.log
```

**Order matters.** The descriptor is opened *before* setting the baud rate. Do
it the other way round and macOS resets the port configuration on open, leaving
the capture unreadable. That happened on 2026-08-06 and cost a whole take.

⚠️ **For long captures, stop the Mac from sleeping:**

```
caffeinate -i bash -c '...the capture...'
```

If the machine sleeps, the process stops reading, the USB driver buffer
overflows and **whatever arrived is lost with no warning**. It happened on
2026-08-06 during a 20 minute capture: a 100 s gap with no traffic appeared and
looked exactly like the finding being hunted, the bus going to sleep, when it
was the computer. What gave it away were the lines torn at the seam and 1.4%
corruption spread around.

**Also turn the raw dump off with `r` before a long capture.** At 25
transactions per second for 20 minutes there is a lot to lose through the port;
with the dump off almost nothing gets printed and the statistics are enough.

Adjust the port name with `ls /dev/cu.*` if it changes. Stop with Ctrl-C.

The first bytes of every capture are garbage: it is the ESP32 bootloader banner,
emitted at 115200 while the port is being read at 460800. That is normal and
**it does not get trimmed**, because the policy says captures are not edited.

---

## Existing captures

| File | What it holds |
|---|---|
| [2026-08-06-movimiento-subir.log](2026-08-06-movimiento-subir.log) | First capture with the protocol already readable. Height from 080 to 087 and back via preset. Proves that **height refreshes during movement** |
| [2026-08-06-pulsadores.log](2026-08-06-pulsadores.log) | Each button on its own. Gives the key codes plus a full run from 077 to 117 that verifies the hundreds digit |
| [2026-08-06-umbral-grabar-memoria.log](2026-08-06-umbral-grabar-memoria.log) | Measurement of the preset-write threshold: **3.0 s** |
| [2026-08-06-umbral-toque-vs-continuo.log](2026-08-06-umbral-toque-vs-continuo.log) | Threshold separating a tap from continuous travel: **2.2 to 2.6 s**. Basis for [ADR-023](../DECISIONS.md) |
| [2026-08-06-topes-fisicos.log](2026-08-06-topes-fisicos.log) | Full travel. Real range **73 to 118 cm**, and hitting the stop looks no different from standing still |
| [2026-08-06-reposo-largo.log](2026-08-06-reposo-largo.log) | **Useless.** The Mac slept halfway through. Kept as the example of why `caffeinate` is needed |
| [2026-08-06-reposo-largo-2.log](2026-08-06-reposo-largo-2.log) | Correct retake: 15 min idle with the dump off and the Mac awake |
