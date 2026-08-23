#!/usr/bin/env python3
"""Comprueba que los cuatro canales siguen accionando su boton.

Para usar despues de manipular el hardware -- abrir el mando, mover cables,
poner la tapa. Dispara cada canal y lee del bus que tecla vio la caja.

Los canales de memoria (3 y 4) arrancan un viaje con cualquier toque, asi que
en cuanto delatan su codigo se FRENAN con un toque en otro canal. Evita
recorridos largos y deja el escritorio donde estaba, aproximadamente.

Desde el 2026-08-23 los digitos por serie SIEMPRE son toques de 300 ms en el
firmware (no mueven el escritorio, ADR-027), asi que ya no hay que cambiar
PULSE_MS para el barrido.

Uso:  tools/verificar_canales.py
"""
import os, re, select, sys, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from serial_talk import open_port, PORT

ESPERADO = {1: ('0x47', 'subir'), 2: ('0x57', 'bajar'),
            3: ('0x67', 'memoria 1'), 4: ('0x6F', 'memoria 2')}
MEMORIAS = (3, 4)
ANSWER = re.compile(r'CHANNEL (\d) answered with (0x[0-9A-F]{2})')
DISP   = re.compile(r'>>> DISPLAY: "(\d{3})\s*"')

fd = open_port(PORT, 115200)
buf, altura = b"", None


def pump(sec, stop_on_match=False):
    global buf, altura
    visto = None
    end = time.time() + sec
    while time.time() < end:
        if stop_on_match and visto:
            return visto   # identified: stop listening NOW so the brake fires sooner
        r, _, _ = select.select([fd], [], [], 0.05)
        if not r:
            continue
        try:
            d = os.read(fd, 4096)
        except BlockingIOError:
            continue
        if not d:
            continue
        buf += d
        parts = buf.split(b"\n")
        buf = parts[-1]
        for ln in parts[:-1]:
            t = ln.decode('utf-8', 'replace')
            m = ANSWER.search(t)
            if m:
                visto = (int(m.group(1)), m.group(2))
            m2 = DISP.search(t)
            if m2:
                v = int(m2.group(1))
                if 73 <= v <= 118:
                    altura = v
    return visto


pump(4.0)
os.write(fd, b'w'); pump(3.0)          # despertar para tener altura
print("altura al empezar: %s cm\n" % altura)

fallos = []
for ch in (1, 2, 3, 4):
    code, nombre = ESPERADO[ch]
    os.write(fd, str(ch).encode())
    visto = pump(6.0, stop_on_match=True)
    if ch in MEMORIAS:
        # Brake IMMEDIATELY after identification -- waiting the full window let
        # the preset trip run ~5 s (~3.4 cm) unsupervised (round-2 review).
        os.write(fd, b'2')             # 300 ms tap: brakes without moving
        pump(3.0)
    if visto is None:
        print("  canal %d (%-9s): SIN RESPUESTA  <-- revisar" % (ch, nombre))
        fallos.append(ch)
    elif visto[1].upper() == code.upper():
        print("  canal %d (%-9s): %s  OK" % (ch, nombre, visto[1]))
    else:
        print("  canal %d (%-9s): %s  <-- ESPERABA %s" % (ch, nombre, visto[1], code))
        fallos.append(ch)

print("\naltura al terminar: %s cm" % altura)
print("RESULTADO: %s" % ("LOS CUATRO OK" if not fallos else "FALLAN LOS CANALES %s" % fallos))
os.close(fd)
sys.exit(0 if not fallos else 1)
