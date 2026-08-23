#!/usr/bin/env python3
"""
Habla con desk_sniffer: manda un comando y graba lo que responde el bus.

Por que existe (2026-08-21): las redirecciones de shell NO consiguen que los
bytes lleguen al sketch. macOS reinicia la configuracion del puerto en cada
open(), y la velocidad hay que fijarla con el ioctl nativo IOSSIOSPEED sobre el
MISMO descriptor que se usa para leer y escribir. Ver ADR-025.

Uso:
    tools/serial_talk.py                      solo escuchar 10 s
    tools/serial_talk.py -c 2                 disparar el canal 2
    tools/serial_talk.py -c 2 -o captura.log  y guardar la salida

El canal se dispara a los 3 s, y despues escucha 9 s mas — el sketch vigila el
bus 1.5 s tras el pulso buscando que tecla vio la caja de control.
"""
import argparse, fcntl, os, select, struct, sys, termios, time

PORT = "/dev/cu.usbserial-0001"
BAUD = 115200          # ADR-026: 460800 tampoco recibio comandos
IOSSIOSPEED = 0x80045402


def open_port(port, baud):
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    attrs = termios.tcgetattr(fd)
    cc = list(attrs[6])
    cc[termios.VMIN] = 0
    cc[termios.VTIME] = 0
    # raw total: sin traduccion de fin de linea, sin eco, sin control de flujo
    termios.tcsetattr(fd, termios.TCSANOW,
                      [0, 0,
                       termios.CREAD | termios.CLOCAL | termios.CS8,
                       0, termios.B9600, termios.B9600, cc])
    fcntl.ioctl(fd, IOSSIOSPEED, struct.pack('I', baud))
    return fd


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('-c', '--command', help="comando a enviar: 1-4, h, s, r, l, c")
    ap.add_argument('-o', '--output', help="archivo donde anadir la salida")
    ap.add_argument('-p', '--port', default=PORT)
    ap.add_argument('-b', '--baud', type=int, default=BAUD)
    ap.add_argument('--pre', type=float, default=3.0,
                    help="segundos minimos antes de enviar")
    ap.add_argument('--post', type=float, default=9.0, help="segundos de escucha despues")
    ap.add_argument('--no-wait-boot', action='store_true',
                    help="no esperar el banner de arranque antes de enviar")
    a = ap.parse_args()

    fd = open_port(a.port, a.baud)
    buf = bytearray()
    t0 = time.time()
    sent = a.command is None
    total = a.pre + a.post

    # Abrir el puerto RESETEA la placa (DTR/RTS del adaptador USB). El sketch
    # tarda ~2.5 s en arrancar —300 ms de espera mas 2 s de checkLines()— y
    # durante ese rato NO lee el puerto: un comando enviado antes se pierde.
    # Por eso se espera a ver la ultima linea del arranque.
    BOOT_DONE = b"Listening."
    booted = a.no_wait_boot

    try:
        while time.time() - t0 < total:
            el = time.time() - t0
            if not booted and BOOT_DONE in buf:
                booted = True
                sys.stderr.write("[sketch arrancado a los %.1f s]\n" % el)
                total = el + a.pre + a.post   # cuenta desde que esta listo
            if not booted and el >= a.pre + 6:
                booted = True   # banner never seen: the board may not have reset
                sys.stderr.write("[banner no visto: envio a ciegas]\n")
            if not sent and booted and el >= a.pre:
                os.write(fd, a.command.encode())
                sent = True
                sys.stderr.write("[enviado %r a los %.1f s]\n" % (a.command, el))
            r, _, _ = select.select([fd], [], [], 0.1)
            if r:
                try:
                    d = os.read(fd, 4096)
                    if d:
                        buf.extend(d)
                except BlockingIOError:
                    pass
    finally:
        os.close(fd)

    text = buf.decode('utf-8', errors='replace')
    if a.output:
        with open(a.output, 'a') as f:
            f.write(text)
        sys.stderr.write("[%d bytes anadidos a %s]\n" % (len(buf), a.output))
    else:
        sys.stdout.write(text)


if __name__ == '__main__':
    main()
