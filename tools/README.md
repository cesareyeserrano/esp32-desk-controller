# Herramientas

Scripts de apoyo. No forman parte del firmware.

## `serial_talk.py`

Manda un comando a `desk_sniffer` y graba la respuesta del bus.

```
tools/serial_talk.py -c 2 -o docs/capturas/2026-08-21-canal2.log
```

**Existe porque las redirecciones de shell no funcionan para escribir.** macOS
reinicia la configuración del puerto en cada `open()`, y la velocidad hay que
fijarla con el ioctl nativo `IOSSIOSPEED` sobre el **mismo** descriptor que se
usa para leer y escribir. Con `printf '2' > /dev/cu.usbserial-0001` los bytes no
llegan nunca al sketch — comprobado el 2026-08-21 con tres herramientas
distintas.

Para **solo escuchar**, la receta de `stty` de
[../docs/capturas/README.md](../docs/capturas/README.md) sigue valiendo.

Velocidad por defecto **115200**, no 921600 ni 460800: ver
[ADR-026](../docs/DECISIONS.md), que corrige al
[ADR-025](../docs/DECISIONS.md).

⚠️ **Mandar `h` y comprobar que responde la ayuda antes de fiarse de cualquier
prueba de canal.** El puerto dejó de recibir comandos sin causa identificada el
2026-08-21, y eso hizo parecer muerto un canal que no se había llegado a probar.

**Cerrar el Arduino IDE antes.** Su Serial Monitor agarra el puerto y solo un
proceso puede tenerlo. Se comprueba con `lsof /dev/cu.usbserial-0001`.
