#!/usr/bin/env python3
"""
Dispara un canal de accionamiento en bucle, para poder medirlo con el multimetro.

Por que existe (2026-08-21): un pulso suelto dura 300 ms, demasiado poco para
que un multimetro asiente la lectura. Repitiendolo al 60% de ciclo de trabajo,
el aparato promedia y da un numero estable.

SEGURIDAD: manda toques de 300 ms separados por 1.5 s. NUNCA un cierre largo.
Un cierre continuo arrancaria movimiento continuo a los 2.2 s y grabaria un
preset a los 3.0 s. Ver ADR-023.

Corregido en la revision del 2026-08-23 (ronda 2): la cadencia era 0.5 s y los
pulsos ya duraban mas que eso -- los bytes se encolaban y los cierres se
pegaban casi sin hueco, acercandose a una tecla MANTENIDA. Ahora la cadencia
deja hueco de sobra, y ademas los digitos por serie volvieron a 300 ms en el
firmware.

Uso:
    tools/pulse_loop.py            canal 2 durante 60 s
    tools/pulse_loop.py 1 120      canal 1 durante 120 s

Lecturas esperadas con el canal sano, en tension continua contra GND:
    pin del GPIO del canal .... 1.5 - 2.2 V
    patas 1 y 2 del PC817 ..... 0.6 - 0.9 V
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from serial_talk import open_port, PORT

BAUD = 115200          # ADR-026
PERIOD_S = 1.5


def main():
    ch = sys.argv[1] if len(sys.argv) > 1 else '2'
    secs = float(sys.argv[2]) if len(sys.argv) > 2 else 60.0
    if ch not in '1234':
        sys.exit("canal invalido: %r (usa 1, 2, 3 o 4)" % ch)

    fd = open_port(PORT, BAUD)
    time.sleep(3.5)                 # el sketch tarda ~2.5 s en arrancar
    t0 = time.time()
    n = 0
    while time.time() - t0 < secs:
        os.write(fd, ch.encode())
        n += 1
        time.sleep(PERIOD_S)
    os.close(fd)
    print("canal %s: %d disparos en %.0f s" % (ch, n, secs))


if __name__ == '__main__':
    main()
