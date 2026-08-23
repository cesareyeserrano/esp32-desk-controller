# Capturas

Volcados crudos del sniffer. **No se editan.** No se recortan, no se limpian,
no se reordenan. Una captura editada deja de ser evidencia.

La interpretación va en [../PROTOCOLO.md](../PROTOCOLO.md), citando el archivo
y la línea.

## Nombre de archivo

```
AAAA-MM-DD-descripcion-corta.log
```

Ejemplos: `2026-08-03-reposo-altura-74.log`,
`2026-08-03-pulsador-subir.log`.

## Cabecera obligatoria

Toda captura empieza con un bloque de contexto. Una captura sin contexto es un
archivo de números inservible: dentro de un mes nadie recuerda a qué altura
estaba el escritorio ni qué botón se pulsó.

*Corregido el 2026-08-03: el ejemplo de abajo decía `sonda 10k/20k en P16
(amarillo)`. Las dos cosas estaban mal — esa sonda nunca se adoptó y el amarillo
es el hilo de 5 V, que no se conecta jamás. Una plantilla que se copia no puede
llevar el cableado equivocado.*

```
# Fecha: 2026-08-03 18:40
# Altura en pantalla: 74.5
# Estado: escritorio quieto, nadie tocando el mando
# Montaje: sonda 9.1k/27k en P18 (rojo, CLK) y P4 (verde, DIO), ESP32 por USB
# Firmware: sniffer v1
# Qué se esperaba: tráfico periódico de refresco de display
```

## Con qué se generan

**Cerrar antes el Serial Monitor del Arduino IDE** — solo un programa puede
tener el puerto abierto a la vez.

```
exec 3</dev/cu.usbserial-0001
stty -f /dev/cu.usbserial-0001 460800 raw -echo
cat <&3 >> docs/capturas/AAAA-MM-DD-descripcion.log
```

**El orden importa.** El descriptor se abre *antes* de fijar la velocidad: si se
hace al revés, macOS reinicia la configuración del puerto al abrirlo y la
captura sale ilegible. Pasó el 2026-08-06 y se perdió una toma entera.

⚠️ **Para capturas largas, impedir que el Mac se duerma:**

```
caffeinate -i bash -c '...la captura...'
```

Si el equipo se duerme, el proceso deja de leer, el buffer del driver USB se
desborda y **se pierde lo recibido sin previo aviso**. Pasó el 2026-08-06 en una
captura de 20 minutos: apareció un hueco de 100 s sin tráfico que parecía el
hallazgo que se buscaba —el bus durmiéndose— y era el ordenador. Lo delataron las
líneas partidas en la costura y un 1.4 % de corrupción repartido.

**Y para capturas largas, apagar antes el volcado crudo con `r`.** A 25
transacciones por segundo durante 20 minutos hay mucho que perder por el puerto;
con el volcado apagado no se imprime casi nada y las estadísticas bastan.

Ajustar el nombre del puerto con `ls /dev/cu.*` si cambia. Se corta con Ctrl-C.

Los primeros bytes de cada captura son basura: es el mensaje de arranque del
bootloader del ESP32, que sale a 115200 mientras se lee a 460800. Es normal y
**no se recorta** — la política dice que las capturas no se editan.

---

## Capturas existentes

| Archivo | Qué contiene |
|---|---|
| [2026-08-06-movimiento-subir.log](2026-08-06-movimiento-subir.log) | Primera captura con el protocolo ya legible. Altura de 080 a 087 y vuelta con memoria. Demuestra que **la altura se refresca durante el movimiento** |
| [2026-08-06-pulsadores.log](2026-08-06-pulsadores.log) | Cada botón por separado. Da los códigos de tecla y un recorrido completo de 077 a 117 que verifica el dígito de las centenas |
| [2026-08-06-umbral-grabar-memoria.log](2026-08-06-umbral-grabar-memoria.log) | Medición del umbral de grabar preset: **3.0 s** |
| [2026-08-06-umbral-toque-vs-continuo.log](2026-08-06-umbral-toque-vs-continuo.log) | Umbral que separa un toque de un movimiento continuo: **2.2 – 2.6 s**. Base de [ADR-023](../DECISIONS.md) |
| [2026-08-06-topes-fisicos.log](2026-08-06-topes-fisicos.log) | Recorrido completo. Rango real **73 a 118 cm**, y al topar no ocurre nada distinguible de estar parado |
| [2026-08-06-reposo-largo.log](2026-08-06-reposo-largo.log) | **Inservible.** El Mac se durmió a mitad. Se conserva como ejemplo de por qué hace falta `caffeinate` |
| [2026-08-06-reposo-largo-2.log](2026-08-06-reposo-largo-2.log) | Repetición correcta: 15 min de reposo con el volcado apagado y el Mac despierto |
