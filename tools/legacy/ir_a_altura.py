#!/usr/bin/env python3
"""Lleva el escritorio a una altura, en lazo cerrado con el bus. Sube o baja.

Sustituye a goto_height.py, que solo bajaba. La logica es la que se valido en
el recorrido completo del 2026-08-22: movimiento continuo para el tramo largo,
freno por altura leida del bus anticipando la inercia, y toques para el ajuste.

Uso:
    tools/ir_a_altura.py 110
    tools/ir_a_altura.py 95 --crudo captura.log

SEGURIDAD (ADR-028): movimiento continuo, SOLO CON SUPERVISION.
  - Limites duros 73..118.
  - Lecturas incoherentes (saltos > 3 cm) descartadas; si se repiten, FRENA.
  - Si no avanza fuera de un tope, FRENA y aborta.
  - Timeout por tramo.
"""
import os, re, select, sys, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from serial_talk import open_port, PORT

MIN_H, MAX_H = 73, 118
MAX_JUMP     = 3      # cm entre lecturas consecutivas
BRAKE_LEAD   = 1      # frenar 1 cm antes: hay ~1 cm de inercia
TIMEOUT      = 150.0
DISP = re.compile(r'>>> DISPLAY: "(\d{3})\s*"')
UP_LONG, DOWN_LONG, UP_TAP, DOWN_TAP, WAKE = b'A', b'B', b'1', b'2', b'w'


class Desk:
    def __init__(self, raw_path=None):
        self.fd = open_port(PORT, 115200)
        self.raw = open(raw_path, 'wb') if raw_path else None
        self.buf, self.height, self.bad = b"", None, 0
        self.t0 = time.time()

    def log(self, msg):
        print("[%6.1fs] %s" % (time.time() - self.t0, msg), flush=True)

    def pump(self, seconds):
        end = time.time() + seconds
        while time.time() < end:
            r, _, _ = select.select([self.fd], [], [], 0.05)
            if not r:
                continue
            try:
                d = os.read(self.fd, 4096)
            except BlockingIOError:
                continue
            if not d:
                continue
            if self.raw:
                self.raw.write(d); self.raw.flush()
            self.buf += d
            parts = self.buf.split(b"\n")
            self.buf = parts[-1]
            for ln in parts[:-1]:
                m = DISP.search(ln.decode('utf-8', 'replace'))
                if not m:
                    continue
                h = int(m.group(1))
                if not (MIN_H <= h <= MAX_H):
                    continue                       # refresco parcial
                if self.height is not None and abs(h - self.height) > MAX_JUMP:
                    self.bad += 1
                    continue
                self.bad = 0
                self.height = h
        return self.height

    def wake(self):
        """Toque de 300 ms: despierta el display SIN mover el escritorio."""
        os.write(self.fd, WAKE)
        self.pump(3.0)
        return self.height

    def brake(self, going_down):
        os.write(self.fd, DOWN_TAP if going_down else UP_TAP)
        self.pump(2.5)

    def go(self, target):
        if not (MIN_H <= target <= MAX_H):
            self.log("ABORTA: %d fuera de 73..118" % target); return False
        if self.height is None:
            self.log("display dormido: refrescando")
            if self.wake() is None:
                self.log("ABORTA: sin altura"); return False
        self.log("altura %s cm -> objetivo %d cm" % (self.height, target))
        if self.height == target:
            return True

        going_down = self.height > target
        at_limit   = target in (MIN_H, MAX_H)

        if abs(self.height - target) <= 2:
            for _ in range(12):
                os.write(self.fd, DOWN_TAP if going_down else UP_TAP)
                self.pump(1.6)
                if (going_down and self.height <= target) or \
                   (not going_down and self.height >= target):
                    break
            self.log("llego a %s cm (toques)" % self.height); return True

        os.write(self.fd, DOWN_LONG if going_down else UP_LONG)
        self.log("movimiento continuo %s" % ("bajando" if going_down else "subiendo"))
        self.pump(3.2)

        t, last, still = time.time(), self.height, 0
        while time.time() - t < TIMEOUT:
            self.pump(0.4)
            if self.bad >= 5:
                self.log("FRENA: lecturas incoherentes"); self.brake(going_down); return False
            if self.height == last:
                still += 1
                if still > 30:
                    if at_limit:
                        self.log("tope alcanzado en %s cm" % self.height); return True
                    self.log("FRENA: no avanza"); self.brake(going_down); return False
            else:
                still = 0; last = self.height
            if at_limit:
                continue
            if going_down and self.height <= target + BRAKE_LEAD:
                self.brake(True); break
            if not going_down and self.height >= target - BRAKE_LEAD:
                self.brake(False); break
        else:
            self.log("FRENA: timeout"); self.brake(going_down); return False

        for _ in range(6):
            if self.height == target: break
            os.write(self.fd, DOWN_TAP if self.height > target else UP_TAP)
            self.pump(1.6)
        self.log("llego a %s cm" % self.height)
        return True

    def close(self):
        if self.raw: self.raw.close()
        os.close(self.fd)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    target = int(sys.argv[1])
    raw = sys.argv[3] if len(sys.argv) > 3 and sys.argv[2] == '--crudo' else None
    d = Desk(raw)
    d.pump(4.0)
    ok = d.go(target)
    d.log("FIN: %s cm. %s" % (d.height, "OK" if ok else "ABORTADO"))
    d.close()
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
