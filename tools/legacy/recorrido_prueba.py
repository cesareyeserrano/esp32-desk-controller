#!/usr/bin/env python3
"""Prueba completa de recorrido, en lazo cerrado con el bus.

Usa movimiento continuo para los tramos largos y lo frena leyendo la altura
real, con toques para el ajuste fino. Guarda el volcado CRUDO del bus.

SEGURIDAD (ADR-028):
  - Solo con supervision. El mando fisico para con cualquier boton.
  - Limites duros 73..118. Fuera de ahi, aborta.
  - Lecturas incoherentes (saltos > 3 cm) se descartan; si se repiten, FRENA
    (ADR-012: con altura obsoleta no se sigue).
  - Timeout por tramo. Si no avanza, FRENA y aborta.
"""
import os, re, select, sys, time

sys.path.insert(0, '/Users/cesareyeserrano/PROJECTS/ESP32/tools')
from serial_talk import open_port, PORT

RAW = sys.argv[1] if len(sys.argv) > 1 else '/tmp/recorrido.log'
MIN_H, MAX_H = 73, 118
MAX_JUMP     = 3       # cm entre lecturas consecutivas
BRAKE_LEAD   = 1       # frenar 1 cm antes: hay inercia
TRAMO_TO     = 150.0   # segundos por tramo
DISP = re.compile(r'>>> DISPLAY: "(\d{3})\s*"')

UP_LONG, DOWN_LONG, UP_TAP, DOWN_TAP = b'A', b'B', b'1', b'2'

fd  = open_port(PORT, 115200)
raw = open(RAW, 'wb')
buf, height, bad = b"", None, 0
t_start = time.time()

def pump(seconds):
    """Lee el puerto, vuelca crudo y actualiza la altura filtrada."""
    global buf, height, bad
    end = time.time() + seconds
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.05)
        if not r:
            continue
        try:
            d = os.read(fd, 4096)
        except BlockingIOError:
            continue
        if not d:
            continue
        raw.write(d); raw.flush()
        buf += d
        parts = buf.split(b"\n")
        buf = parts[-1]
        for ln in parts[:-1]:
            m = DISP.search(ln.decode('utf-8', 'replace'))
            if not m:
                continue
            h = int(m.group(1))
            if not (MIN_H <= h <= MAX_H):
                continue                        # refresco parcial del display
            if height is not None and abs(h - height) > MAX_JUMP:
                bad += 1                        # salto incoherente
                continue
            bad = 0
            height = h
    return height

def brake(direction):
    os.write(fd, DOWN_TAP if direction == 'down' else UP_TAP)
    pump(2.5)

def log(msg):
    print("[%6.1fs] %s" % (time.time() - t_start, msg), flush=True)

def travel_to(target, label):
    global height
    log("--- %s: objetivo %d cm (ahora %s) ---" % (label, target, height))
    if height is None:
        log("ABORTA: sin altura"); return False
    if not (MIN_H <= target <= MAX_H):
        log("ABORTA: objetivo fuera de limites"); return False

    going_down = height > target
    direction  = 'down' if going_down else 'up'
    gap = abs(height - target)
    if gap == 0:
        log("ya esta en %d cm" % target); return True

    at_limit = target in (MIN_H, MAX_H)

    if gap <= 2:                                 # tramo corto: solo toques
        for _ in range(12):
            os.write(fd, DOWN_TAP if going_down else UP_TAP)
            pump(1.6)
            if (going_down and height <= target) or (not going_down and height >= target):
                break
        log("llego a %s cm (toques)" % height)
        return True

    os.write(fd, DOWN_LONG if going_down else UP_LONG)
    log("pulso largo %s -- movimiento continuo" % direction)
    pump(3.2)                                    # dura 2.8 s

    t0, last_h, still = time.time(), height, 0
    while time.time() - t0 < TRAMO_TO:
        pump(0.4)
        if bad >= 5:
            log("FRENA: 5 lecturas incoherentes seguidas"); brake(direction); return False
        if height is None:
            log("FRENA: se perdio la altura"); brake(direction); return False
        if height == last_h:
            still += 1
            if still > 30:                       # ~12 s sin moverse
                if at_limit:
                    log("parado en %s cm: tope alcanzado" % height); return True
                log("FRENA: no avanza (%s cm)" % height); brake(direction); return False
        else:
            still = 0; last_h = height
        if at_limit:
            continue                             # al tope se le deja llegar
        if going_down and height <= target + BRAKE_LEAD:
            brake('down'); log("frenado en %s cm" % height); break
        if not going_down and height >= target - BRAKE_LEAD:
            brake('up');   log("frenado en %s cm" % height); break
    else:
        log("FRENA: timeout del tramo"); brake(direction); return False

    for _ in range(6):                           # ajuste fino
        if height == target: break
        if height > target:  os.write(fd, DOWN_TAP)
        else:                os.write(fd, UP_TAP)
        pump(1.6)
    log("%s -> %s cm" % (label, height))
    return True

pump(4.0)
if height is None:
    log("display dormido: un toque para despertarlo")
    os.write(fd, DOWN_TAP); pump(3.0)
log("altura inicial: %s cm" % height)

PLAN = [(73, "1. bajar del todo"), (80, "2. subir a 80"), (75, "3. bajar a 75"),
        (118, "4. subir del todo"), (95, "5. bajar y parar a media altura"),
        (73, "6. bajar del todo")]

ok = True
for target, label in PLAN:
    if not travel_to(target, label):
        ok = False
        log("SECUENCIA INTERRUMPIDA"); break
    pump(1.5)

log("FIN. altura final %s cm. %s" % (height, "COMPLETA" if ok else "INTERRUMPIDA"))
raw.close(); os.close(fd)
