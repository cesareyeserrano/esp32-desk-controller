# Firmware

## `desk_sniffer/`

Sniffer pasivo del bus del mando. **Solo escucha, nunca escribe** — los dos
pines quedan como entrada de principio a fin, que es lo que exige
[ADR-011](../docs/DECISIONS.md).

### Antes de conectarlo

1. Divisores montados en rojo y en verde: **9.1 kΩ arriba, 27 kΩ abajo**
   ([ADR-016](../docs/DECISIONS.md)). Esquema en
   [HARDWARE.md](../docs/HARDWARE.md).
2. Derivaciones soldadas al conector del mando **con el conector original
   puesto**.
3. **Comprobar que el mando sigue funcionando** con las derivaciones soldadas,
   antes de conectar el ESP32. Si no se comprueba en ese orden, un fallo
   posterior es imposible de atribuir.
4. **CLK va a P18 y DIO a P4**, los dos en la misma columna de la bornera
   ([ADR-020](../docs/DECISIONS.md)). Desde P4, alejándose de P0: P16, P17, P5,
   P18. El GND más cómodo está dos posiciones más allá de P18, pasando P19.
   Mapa completo en [HARDWARE.md](../docs/HARDWARE.md). **Cuidado con `CLK`,
   `SD0`, `SD1`, `SD2` y `SD3` de esa misma bornera: son la flash interna y
   usarlos impide arrancar.**
5. **ESP32 alimentado por USB desde el Mac, y alimentado ANTES de conectar los
   hilos al divisor.** Con el ESP32 sin corriente y la sonda sobre un bus
   encendido, el bus se hunde a ~2.85 V y el mando falla
   ([ADR-019](../docs/DECISIONS.md)). Al desmontar, los hilos primero y el USB
   después. El hilo amarillo no se conecta nunca.
6. **Medir el nivel del bus, y juzgarlo por el cociente**, no por el valor
   absoluto: un multímetro promedia y el bus está conmutando. Se mide rojo↔azul
   y verde↔azul sin la sonda y con ella, con el escritorio quieto y el mismo
   número en pantalla. El cociente debe rondar **0.80**; por debajo de **0.70**
   hay que desconectar. Tabla completa en [HARDWARE.md](../docs/HARDWARE.md),
   razonamiento en [ADR-018](../docs/DECISIONS.md).

### Compilar y cargar

Arduino IDE con soporte ESP32 instalado:

- Placa: **ESP32 Dev Module**
- Puerto: **`/dev/cu.usbserial-XXXX`**. La placa lleva un USB-serie **CP2102**,
  para el que macOS trae driver desde Big Sur — no hay que instalar nada.
  Confirmar con `ls /dev/cu.*` antes y después de enchufar.
- Abrir `desk_sniffer/desk_sniffer.ino` y cargar

**Por qué CLK está en GPIO18 y no en GPIO16.** Una versión anterior de este
archivo afirmaba que el módulo es un WROOM-32. **Eso nunca se comprobó**: el
blindaje no es legible y lo único que hay es la ficha del vendedor. Y en los
módulos **WROVER**, los GPIO 16 y 17 están cableados a la PSRAM y no sirven.

En vez de resolver la duda se eliminó: **GPIO18 y GPIO4 están libres en los dos
módulos**, no son pines de arranque y no son de la flash. La pregunta "¿WROOM o
WROVER?" ya no tiene consecuencias aquí. Ver
[ADR-020](../docs/DECISIONS.md).

**Compilado y ejecutado por primera vez el 2026-08-03.** ✅

Arduino IDE con el core de Espressif, placa *ESP32 Dev Module*, sobre el DevKit
real. **Compila sin errores, carga y arranca.** La autocomprobación de líneas
corre y reporta correctamente, con el ESP32 alimentado solo por USB y **nada
conectado al escritorio**:

```
Identifying lines (2 s)...
  GPIO18 (expected CLK, red)  : 0 edges
  GPIO4 (expected DIO, green): 0 edges
  !! No activity on either line.
```

Cero flancos en las dos líneas es **el resultado esperado sin nada conectado**.

Después se le inyectó ruido tocando GPIO18 con el dedo, con los contadores
reiniciados:

```
  edges captured : 197548
  edges dropped  : 0
  transactions   : 0 (0 malformed, 0 ended by repeated START)
```

**Qué queda demostrado:** que compila con la toolchain de verdad, que arranca,
que el serie a 921600 funciona, que las interrupciones se enganchan y disparan,
que la cola aguanta ~200.000 flancos **sin perder ninguno**, y que el
decodificador **no inventa transacciones a partir de ruido** — la detección de
START/STOP no alucina protocolo donde no lo hay.

**Qué no:** el decodificador contra datos reales. Necesita bus, y eso es la
fase 2.

**Truco de depuración:** la ventana del comando `l` son 2 segundos y hay que
estar ya generando flancos al pulsarlo. Para comprobar un pin sin prisa: `c`,
generar actividad, `s`, y mirar `edges captured`.

## ⚠️ Qué contador vigilar: `malformed`, no `dropped`

Medido el 2026-08-03 con
[test_capture_ceiling](test_capture_ceiling/test_capture_ceiling.ino):

| Onda cuadrada | Flancos esperados | Capturados | `dropped` |
|---|---|---|---|
| 125 kHz | 250.000 | 250.000 | 0 |
| **150 kHz** | 300.000 | **299.613** | **0** |

**A 150 kHz se pierden flancos y `dropped` marca cero.** La causa: cuando dos
flancos llegan más juntos que la duración del ISR, el registro de estado del
GPIO anota *que hubo* interrupción pero no *cuántas*. Los dos colapsan en una
sola llamada y el segundo desaparece sin incrementar ningún contador.

`dropped` solo cuenta los flancos que **llegaron al ISR y no cupieron en la
cola**. No ve los que nunca lo dispararon.

**Regla operativa:** `dropped` en cero **no es** prueba de captura sana. El
canario es **`malformed`**: un flanco perdido a mitad de transacción descuadra la
cuenta de bits y la marca. Ese sí detecta la pérdida silenciosa.

### Corrección: `malformed` no llegó a cero con el muestreo por ráfagas

Se afirmó que el cambio a ráfagas hacía desaparecer las transacciones
malformadas. **No fue así:** quedaba un **0.7–1 %** en todas las capturas del
2026-08-06, y **las 171 tenían exactamente cero bytes**.

No eran tráfico roto: eran **nuestras**. Entre ráfaga y ráfaga el bus sigue
moviéndose sin que nadie mire, y el decodificador —cuyo estado es global—
comparaba el primer cambio de la ráfaga nueva contra un nivel de milisegundos
antes, inventando un START o un STOP en la costura.

Corregido: `decodeBurst()` **resiembra el decodificador** con la primera muestra
de cada grabación y descarta lo que creyera tener a medias, contándolo aparte en
`cut by burst`. Así **`malformed` vuelve a significar "el bus dijo algo que no
supimos leer"** en vez de medir nuestro propio instrumento.

Es la tercera vez en este proyecto que un contador mide la herramienta y no el
mundo. Merece la pena desconfiar de ellos por defecto.

**Pero no llegó a cero: el suelo de ruido es ~0.8 %.** En 15 minutos de reposo
quedaron 175 malformadas de 22.731 transacciones, todas del tipo `S P` sin
bytes, repartidas de forma irregular y con `cut by burst` en cero.

**No son pérdidas: son transacciones de más.** La prueba es que la cuenta de
ciclos cuadra exacta — 18.020 escrituras de dígito ÷ 4 = 4.505 ciclos = 4.505
lecturas de teclado. Ningún ciclo llegó incompleto.

*Hipótesis sin verificar:* a 4 MHz cada muestra dura 250 ns; si DIO cambia dentro
de esa ventana respecto al flanco de bajada de CLK, se ve un cambio de DIO con
CLK aún alto, que por definición es un START o un STOP. Sería resolución de
muestreo. Si algún día estorba, la vía es exigir que CLK lleve varias muestras
estable antes de declarar START o STOP.

**Al interpretar `malformed`, el cero de referencia es ~0.8 %, no 0.**

### Y antes de creerse cualquier contador en cero

El sketch de la prueba nació con el criterio de éxito puesto en `dropped == 0`,
y **daba "clean up to 400 kHz" con el cable desconectado**: sin señal no hay nada
que descartar, así que el contador de fallos se queda en cero y todo parece
perfecto. Corregido el 2026-08-03 — ahora exige que los flancos esperados hayan
llegado de verdad.

Es la misma trampa que la revisión del 2026-08-02 encontró en `checkLines()`, que
daba OK con un cable cortado. **Un contador de errores en cero puede significar
"todo bien" o "no hay nadie ahí".** Al interpretar una captura de la fase 2, antes
de dar por buena cualquier estadística: comprobar que hubo tráfico que contar.

**Techo medido:** limpio hasta **125 kHz de onda cuadrada = 250.000 flancos/s**.
Como los flancos llegan al doble de la frecuencia del reloj del bus, eso cubre un
bus de hasta ~125 kHz. Esta familia de chips suele correr muy por debajo.

*Antes de esto solo había pasado un análisis sintáctico con clang y cabeceras
simuladas (`-fsyntax-only -Wall -Wextra`, cero avisos), que descartaba erratas y
errores de tipo pero no desajustes con las firmas reales del core.*

### Actualizar por red (OTA) — desde el 2026-08-23

**El ESP32 vive en un cargador de pared: no hace falta el cable.** La IP está
publicada como entidad `IP` en Home Assistant (a día de hoy `192.168.1.23`), y
la contraseña, en `secrets.h`.

```
CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
"$CLI" upload -p 192.168.1.23 --fqbn esp32:esp32:esp32 \
  --upload-field password=<OTA_PASSWORD> firmware/desk_sniffer
```

⚠️ **La actualización se rechaza si el escritorio está en movimiento**
([ADR-034](../docs/DECISIONS.md)): reiniciar abre los canales, y abrir un
contacto no detiene el movimiento continuo.

Un `[WARNING]: Unexpected response from device: '256'` al final es cosmético del
script de subida; si se vio `100% Done`, la actualización se aplicó — se
confirma mirando que el `uptime` en HA vuelve a empezar.

**El cable sigue haciendo falta para:** el primer flasheo de una placa nueva, y
para leer el puerto serie.

### Monitor serie

**115200 baudios.** *(Corregido dos veces el 2026-08-21: decía 921600, pasó a
460800 y acabó aquí. A 921600 la placa transmite bien pero no recibe ni un
comando; a 460800 tampoco recibió, pese a lo que dice el
[ADR-025](../docs/DECISIONS.md). **La causa del fallo de recepción sigue sin
identificarse** — la tabla completa de lo que se descartó está en el
[ADR-026](../docs/DECISIONS.md).)*

⚠️ **Corregido también el argumento, que estaba mal.** Este párrafo decía que
*"el display se refresca cada ~8 ms, así que a 115200 el puerto sería el cuello
de botella"*. **Medido el 2026-08-21 sobre las capturas: el ciclo de refresco es
de 200 ms, no de 8.** Los cinco mensajes de un ciclo salen en 1.5 ms y después
la línea queda en silencio hasta los 200 ms.

El caudal real en reposo son **1.4 KB/s de los 11.5 KB/s que da 115200**, un 12%.
Sobra de largo, y el margen que se creía necesario nunca lo fue.

**Donde sí aprieta es con el volcado crudo (`r`) activo**, que es el modo que más
escupe. Si al usarlo aparecen líneas perdidas, la respuesta no es subir la
velocidad a ciegas: primero comprobar que a esa velocidad se siguen recibiendo
comandos, mandando `h`.

Para capturar a archivo:

```
screen -L -Logfile docs/capturas/2026-08-02-reposo.log /dev/cu.usbserial-XXXX 115200
```

Salir de `screen`: `Ctrl-A` y luego `K`.

Después de capturar, **añadir la cabecera de contexto al archivo** — formato en
[capturas/README.md](../docs/capturas/README.md). Una captura sin contexto no
sirve.

### Qué hace al arrancar

Cuenta flancos en las dos líneas durante 2 segundos y dice si el cableado
tiene sentido. CLK lleva una ráfaga de pulsos por transacción y DIO cambia como
mucho una vez por bit, así que si DIO tiene más flancos que CLK, los cables
están cruzados. Es el error de montaje más probable y produce basura que parece
un problema de protocolo.

Si no ve actividad en ninguna línea: revisar GND, los divisores, y que el
escritorio esté encendido.

### Formato de salida

```
[     3.482910] S 68a 07a P  | DIG1 seg=0x07 '7'
[     3.483514] S 6Aa E6a P  | DIG2 seg=0xE6 '4' +DP
[     3.484102] S 6Ca 6Da P  | DIG3 seg=0x6D '5'
[     3.484688] >>> DISPLAY: "74.5"
[     3.520044] S 49a 2E- P  | KEY none
```

- `S` … `P` son START y STOP.
- Cada byte lleva su ACK: `a` = reconocido (línea baja), `-` = línea alta.
- **En las lecturas de teclado, el `-` del segundo byte es normal**, no un
  fallo: el datasheet dice que en una lectura el noveno bit del comando es 0 y
  el del dato es 1. En las escrituras los dos bytes deberían salir con `a`.
- Las líneas `>>>` son eventos: cambio de lo que muestra la pantalla, o tecla
  pulsada. Salen aunque el volcado crudo esté apagado.

### Comandos por el puerto serie

| Tecla | Efecto |
|---|---|
| `r` | Enciende o apaga el volcado crudo de cada transacción |
| `s` | Estadísticas: flancos, transacciones, velocidad del bus, descartes |
| `c` | Reinicia las estadísticas |
| `l` | Repite la comprobación de líneas |
| `h` | Ayuda |

### Qué mirar en las estadísticas

**`edges dropped`** significa que se llenó el buffer y se perdieron flancos.
Ojo con la interpretación: **puede ser culpa nuestra**. Con el volcado crudo
encendido, imprimir cada transacción puede saturar el puerto serie y bloquear
el bucle mientras la interrupción sigue llenando la cola. Para saber si el bus
de verdad va más rápido de lo que capturamos, **apaga el volcado con `r` y
vuelve a mirar**. El propio mensaje de estadísticas te lo recuerda.

**`malformed`** cuenta transacciones que no traen exactamente dos bytes. El
datasheet dice que todas son de 16 bits, así que cualquier otra cosa es un
flanco perdido. Unas pocas al empezar a escuchar son normales —se entra a mitad
de una transacción—; muchas y sostenidas, no.

**`ended by repeated START`** cuenta transacciones que terminaron con otro
START en vez de con un STOP. El datasheet no describe ese caso, así que si
aparece de forma sistemática es que el maestro hace algo que no esperábamos y
hay que mirarlo antes de fiarse de la decodificación.

**`fastest clock`** es el dato que decide si el divisor resistivo aguanta. La
impedancia que manda es la del **flanco de subida**, que en la sonda de
[ADR-016](../docs/DECISIONS.md) es de **10.9 kΩ**, no los 6.8 kΩ que decía este
archivo — ese valor solo vale para el flanco de bajada
([ADR-018](../docs/DECISIONS.md)). Sigue siendo mejor que las versiones
anteriores de la sonda, pero con menos margen del que se creía.

No hay un umbral de kHz que se pueda dar por cálculo, porque depende de la
capacidad parásita del montaje —longitud de los hilos, protoboard— que no se ha
medido. **El criterio es empírico:** si `malformed` se mantiene en cero y los
bytes decodifican, el divisor sirve a la velocidad que haya. Si aparecen bytes
perdidos de forma sostenida con el volcado crudo apagado, el remedio es el
buffer 74LVC2G17 declarado en [ADR-018](../docs/DECISIONS.md) — **nunca** bajar
el valor de las resistencias, que carga el bus.

Se mide en ciclos de CPU, solo dentro de una transacción y reiniciando la cuenta
en cada START, para que ni el tiempo muerto entre transacciones ni un
desbordamiento del contador puedan falsearlo.

### Revisión adversarial

El código pasó una revisión buscando fallos el 2026-08-02, que encontró ocho.
Los dos serios: el contador de ciclos se desbordaba tras 17.9 s de silencio en
el bus —justo el caso de [ADR-012](../docs/DECISIONS.md), cuando el mando se
duerme— y la comprobación de líneas daba "OK" con un cable cortado. Ambos
corregidos. Detalle en la [bitácora](../docs/BITACORA.md).

Una segunda revisión el 2026-08-03 encontró dos más, los dos herederos del
mismo desbordamiento de 17.9 s que la primera creyó cerrado:

1. **El periodo de reloj mínimo se podía envenenar.** La cuenta se reiniciaba
   por transacción según el comentario, pero no según el código: el primer
   flanco de cada trama medía hacia atrás hasta la trama anterior, cruzando el
   hueco muerto. Si ese hueco pasaba de 17.9 s —el chip dormido— la resta daba
   la vuelta y dejaba un mínimo falso y minúsculo, en el único número que
   decide si el divisor sirve.
2. **La marca de tiempo podía saltar 17.9 s hacia adelante.** Si la
   interrupción encolaba un evento justo después de que el bucle diera la cola
   por vacía, el *keep-alive* adelantaba la base por delante de ese evento y la
   resta sin signo lo convertía en un salto enorme, permanente a partir de ahí.

Ambos corregidos. Ninguno de los dos podía dañar nada —el firmware sigue sin
escribir jamás en el bus— pero los dos corrompían datos de captura en silencio,
que es el modo de fallo que este proyecto menos se puede permitir.

### Qué está sin verificar

Todo. Este firmware está escrito contra el datasheet, no contra el bus real.
Nunca se ha ejecutado con hardware conectado. Las siete preguntas abiertas de
[PROTOCOLO.md](../docs/PROTOCOLO.md) siguen abiertas.

---

# test_output_channels — comprobación del accionamiento

Prueba los cuatro canales de accionamiento **antes de soldar nada al mando**.

**Nada de esto toca el escritorio.** Los optoacopladores van en la protoboard
con su lado de salida **conectado a nada**, así que lo peor que puede hacer este
sketch es encender un LED infrarrojo dentro de un chip.

## Qué demuestra, por orden de importancia

1. **Que los pines están bajos en el reset y siguen bajos durante el arranque.**
   Todo [ADR-024](../docs/DECISIONS.md) se apoya en esto: el watchdog nos
   protege reiniciando el chip, y un reinicio solo sirve si deja los canales
   abiertos. Si un pin se pusiera alto al arrancar, el watchdog **activaría** un
   botón en vez de soltarlo — justo lo contrario de la protección.
2. Que un pulso ordenado dura lo que debe y ni un milisegundo más.
3. Que nunca hay dos canales activos a la vez.

## Cableado, por canal

**Plano: [plano_canal_pc817.svg](../docs/hardware/plano_canal_pc817.svg).**

```
GPIO --[330 ohm]--|>|-- PC817 pata 1 (anodo)
                        PC817 pata 2 (catodo) -- GND
  |
[10k]      pull-down: sin el, un pull-up interno debil al arrancar (~45 kOhm)
  |        metería ~47 uA por el LED, incomodamente cerca de los ~90 uA que el
 GND       mando necesita para ver una tecla. Con el, esa fuga se queda en
           0.6 V, muy por debajo de los 1.2 V que el LED necesita para conducir.
```

Multímetro **en las patas 3 y 4 del PC817** para vigilar el canal.

## Pines

**GPIO 27, 26, 25 y 33**, cuatro seguidos en la bornera derecha. Ninguno es pin
de arranque (0, 2, 5, 12, 15), ninguno es de la flash (6–11), y los cuatro
pueden ser salida — al contrario que P34, P35, SVN y SVP de esa misma columna,
que son **solo entrada**.

## Qué debe salir

| Momento | Multímetro en las patas 3-4 |
|---|---|
| EN apretado, chip en reset | **Abierto**, megaohmios |
| Soltando EN, durante el arranque | **Sin pitido** en modo continuidad |
| En reposo tras arrancar | **Abierto** |
| Mientras pulsa (tecla `1`) | **Conduce** brevemente |

Comandos: `1` `2` `3` `4` para pulsar un canal, `l` para ver el nivel de los
cuatro pines, `h` para la ayuda.
