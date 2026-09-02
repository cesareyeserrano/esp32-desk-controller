# Plan

> Fases y estado. Se actualiza al abrir o cerrar una fase, o cuando cambia el
> alcance. El detalle de por qué está en [DECISIONS.md](DECISIONS.md).

**Fase actual: 4 — ESPHome / Home Assistant.** La fase 3 se cerró el
2026-08-22: los cuatro canales verificados contra el bus.

*(Antes decía: fase 3 — Accionar.)* La fase 2 quedó cerrada el 2026-08-06: el
protocolo está descifrado y la altura se lee del bus, verificada contra la
pantalla del mando.

---

## Siguiente acción concreta

✅ **Los recordatorios de postura funcionan de punta a punta** (verificado el
2026-08-24: 45 min sentado → aviso → margen → re-verificación de presencia →
el escritorio sube solo a 117 → botón para deshacer). Es lo primero que el
sistema hace útil por su cuenta.

**El sistema está estable y en periodo de observación (decisión del
propietario, 2026-08-23).** Todo lo esencial de la fase 4 funciona: 21
entidades en HA, viajes con límites y freno verificado, cesión al mando manual,
monitoreo con alertas al móvil, OTA, y el repositorio público en GitHub.

**Al retomar, por orden de valor:**

1. **Presets con nombre** ("de pie", "sentado") — el alcance ya está decidido
   en la sección de fase 4; es configuración sobre lo que ya funciona
2. **Fabricar la placa definitiva** — especificación completa y esquema listos
   en [hardware/PCB_ESPECIFICACION.md](hardware/PCB_ESPECIFICACION.md) y
   los planos en [hardware/](hardware/) (tres láminas `plano_pcb_*.svg`). **Incluye el buffer
   de aislamiento** ([ADR-031](DECISIONS.md)), que es la razón principal de
   rehacerla: la protoboard fue el eslabón frágil del proyecto entero.

   ⚠️ **Antes de encargarla, montar el buffer en protoboard y probarlo**: es la
   única parte del diseño que nunca se ha probado
3. **Estadísticas de uso en HA** (tiempo de pie/sentado — falta decidir el
   umbral en cm) e InfluxDB para histórico largo
4. Cabos sueltos menores: los cinco bytes de reposo sin identificar, y las
   fotografías del chip pendientes de archivar

✅ **Revisión adversarial del 2026-08-23: 16 hallazgos, 16 arreglados y
verificados en vivo** ([REVISION_FIRMWARE_2026-08-23.md](REVISION_FIRMWARE_2026-08-23.md)).
Entre ellos: el watchdog de [ADR-024](DECISIONS.md) que nunca se había
implementado, tres botones de HA que nunca funcionaron, el filtro de alturas
transitorias del display, límites en ambas direcciones, freno verificado con
reintento, y `parar` en un canal propio que no puede perderse. **La fase 4 queda
desbloqueada.** Copia del firmware previo en `firmware/backups/`.


✅ **RESUELTO el 2026-08-23: el mando está sano.** Lo que parecía un chip
dañado era un **cortocircuito entre el hilo verde (DIO) y el amarillo (5 V)**,
un puente de estaño de las soldaduras al cerrar la tapa. Deshecho el corto, el
mando funciona con normalidad — se grabaron y recuperaron dos memorias.

⚠️ **EL MANDO VA Y VUELVE, Y NO SE SABE POR QUÉ (2026-08-23).** Deja de
funcionar y vuelve, al menos cinco veces en un día, siempre alrededor de
manipulaciones. **Causa desconocida**: se propusieron cuatro explicaciones y
ninguna está comprobada. Detalle en la [bitácora](BITACORA.md).

⚠️ **Mientras siga yendo y viniendo, ninguna prueba vale.** Un resultado sobre un
montaje intermitente no distingue un fallo real de una casualidad, y así se
perdieron dos días.

**Reconectado por pasos el 2026-08-23 y verificado en un momento en que
funcionaba: el bus se lee y tres de los cuatro canales responden.**

✅ **El canal 3 quedó resuelto el 2026-08-23: era el optoacoplador flojo en la
protoboard.** Asentado bien, responde `0x67` a la primera. **Los cuatro canales
verificados.**

**En curso: rehacer el cableado** en placa perforada en vez de protoboard. La
protoboard fue el eslabón frágil del 2026-08-23 — el cortocircuito
verde-amarillo y el canal 3 suelto salieron de ahí. Ya estaba en la
[lista de compras](COMPRAS.md).

⚠️ **Reglas que no cambian:** **USB primero, hilos del bus después**
([ADR-019](DECISIONS.md), [ADR-031](DECISIONS.md)), y **mandar `h` y comprobar
que responde antes de fiarse de cualquier prueba de canal**
([ADR-026](DECISIONS.md)).


La sonda está montada y funcionando, y **el protocolo está descifrado**. Detalle
completo en [PROTOCOLO.md](PROTOCOLO.md); capturas crudas en
[capturas/](capturas/).

**La fase 2 está cerrada.** Todas sus preguntas tienen respuesta medida.

**La fase 3 ya está desbloqueada.** La decisión de seguridad que la frenaba
quedó cerrada en [ADR-023](DECISIONS.md) el 2026-08-06: **los cuatro canales
llevan el mismo limitador de ancho de pulso de 300 ms**, y el firmware solo
emite toques.

Todos los tiempos están medidos:

| | Duración |
|---|---|
| Mínimo para que el chip vea la pulsación | 160 ms |
| **Ancho de pulso elegido** | **300 ms** |
| Toque → movimiento continuo | 2.2 – 2.6 s |
| Toque → grabar preset | 3.0 s |

**Y el limitador ya está resuelto sin comprar nada:** lo hace el **watchdog del
ESP32** a 1 segundo ([ADR-024](DECISIONS.md)). El monoestable independiente queda
aplazado, en la [lista acumulada de COMPRAS.md](COMPRAS.md).

### Siguiente paso concreto

**Al retomar, empezar por aquí. Un paso cada vez, sin adelantar.**

**Paso 1 — Los cuatro canales en protoboard ✅ HECHO el 2026-08-20.**

Los cuatro montados y verificados uno a uno, 40 pulsos encadenados cada uno:
**OL en reposo, ~150 Ω al pulsar, 300 ms exactos**. Y con los cuatro
optoacopladores puestos, los cuatro pines siguen leyendo **0 al arrancar** — que
es lo que sostiene [ADR-024](DECISIONS.md). Detalle en la
[bitácora](BITACORA.md).

| Canal | Pin |
|---|---|
| 1 | GPIO 27 |
| 2 | GPIO 26 |
| 3 | GPIO 25 |
| 4 | GPIO 33 |

⚠️ **Cargar con `arduino-cli`, no con el IDE.** El Arduino IDE cargaba
sistemáticamente el sketch equivocado:

```
CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
"$CLI" compile --fqbn esp32:esp32:esp32 firmware/<sketch>
"$CLI" upload -p /dev/cu.usbserial-0001 --fqbn esp32:esp32:esp32 firmware/<sketch>
```

<details>
<summary>Cómo era el paso 1</summary>

Montar un solo PC817 con el ESP32, comprobar que se enciende y se apaga cuando
el firmware lo manda, y que **no se enciende solo al arrancar ni al reiniciar**.
El mando no interviene en este paso: el optoacoplador no está conectado a nada
por su lado de salida.

**Plano del montaje:
[hardware/plano_canal_pc817.svg](hardware/plano_canal_pc817.svg).** Razonamiento
en [SEGURIDAD.md](SEGURIDAD.md) y [ADR-024](DECISIONS.md). Pines propuestos:
**P27, P26, P25 y P33**, cuatro seguidos en la columna derecha de la bornera.

El sketch está en
[firmware/test_output_channels/](../firmware/test_output_channels/). Pone los
cuatro pines bajos como primera instrucción de `setup()`, permite activarlos de
uno en uno escribiendo `1`, `2`, `3` o `4`, y **acota cada pulso a 300 ms**
informando de cuánto duró en realidad.

**Al medir, usar ohmios y no continuidad.** Un fototransistor saturado se queda
en unos cientos de ohmios y los zumbadores solo pitan por debajo de 30 o 50, así
que el modo continuidad hace parecer que el canal no funciona cuando sí lo hace.

</details>

**Paso 2 — Identificar los pulsadores del mando ✅ HECHO el 2026-08-20.**
Los cinco localizados y el reset identificado, que es el que **no se cablea
nunca** ([ADR-008](DECISIONS.md)).

**Paso 3 — Cablear un solo canal** a un pulsador y comprobarlo. ✅ **HECHO el
2026-08-21.**

**Canal 2 (GPIO 26) → pulsador de BAJAR. Verificado en el bus:**

```
[2.597976] >>> KEY PRESSED KI3 / DIG4   <== CHANNEL 2 answered with 0x57
```

`0x57` es bajar, justo lo que predecía [PROTOCOLO.md](PROTOCOLO.md). Captura:
[canal2-verificado-0x57](capturas/2026-08-21-canal2-verificado-0x57.log).

⚠️ **Costó una tarde entera por un fallo del sniffer, no del montaje.** El bucle
del pulso estaba **vacío**: el firmware quedaba ciego durante los 300 ms en que
la tecla estaba pulsada, así que el canal funcionaba y la captura decía que no.
Ya está corregido —el bucle captura mientras el contacto está cerrado— pero
conviene saberlo porque **las capturas de esa tarde que dicen "el canal no
responde" están marcadas como engañosas**, no borradas.

**Dos reglas que salieron de ahí, y que valen para el paso 4:**

1. ⚠️ **Mandar `h` y comprobar que el sketch responde antes de cada prueba**
   ([ADR-026](DECISIONS.md)). El puerto serie dejó de recibir sin causa
   identificada.
2. ⚠️ **Si el escritorio se mueve y la captura dice que no ha pasado nada, el
   sospechoso es la captura.** Es exactamente lo que ocurrió.

<details>
<summary>Cómo se cableó, para repetirlo en el paso 4</summary>

⚠️ **Los cables del pulsador van a las patas 3 y 4 del PC817, nunca a la 1 y 2.**
Las 1 y 2 ya llevan el ESP32 —resistencia de 320 Ω y masa—; son el LED interno.
Las 3 y 4 son el interruptor, el lado del mando. **Esa separación es el
aislamiento**: soldar el pulsador a las patas 1 y 2 uniría los 5 V del mando con
el GPIO, que es justo lo que el optoacoplador existe para impedir.

Del pulsador se cogen sus **dos patas en diagonal**, que son el par que cierra
al apretar. Las de un mismo lado están unidas por dentro.

**Comprobar a qué canal va el cable antes de disparar.** El 2026-08-21 estuvo
conectado al canal 1 mientras se disparaba el canal 2 durante buena parte de la
tarde. Disparar los cuatro canales seguidos lo destapa en un minuto.

**La polaridad no importó.** Se invirtieron los dos cables de las patas 3 y 4 y
el comportamiento fue idéntico — el fallo era del firmware. Si un canal nuevo no
responde, invertirlos sigue siendo barato de probar, pero **no es la primera
sospecha**: antes va comprobar el puerto con `h` y a qué canal está conectado.

**Si hace falta medir**, `tools/pulse_loop.py` dispara un canal en bucle —toques
de 300 ms, nunca un cierre largo— para que dé tiempo a leer el multímetro.
Validar el aparato midiendo 3V3 contra GND antes de fiarse de un cero.

Plano: [hardware/plano_canal_pc817.svg](hardware/plano_canal_pc817.svg).

</details>

### Lo que ya se puede hacer con un solo canal

Verificado el 2026-08-21, moviendo el escritorio de verdad:

| Modo | Cómo | Velocidad | Riesgo |
|---|---|---|---|
| **Toques** (`2`) | Pulsos de 800 ms ([ADR-027](DECISIONS.md)) | ~1 cm cada 3 pulsos | Ninguno nuevo |
| **Continuo** (`B`) | Pulso de 2.8 s, frena con un toque ([ADR-028](DECISIONS.md)) | 6 cm en 7 s | ⚠️ Solo con supervisión |

**Los viajes viven en el firmware desde la fase 4**: `ir:N` por MQTT o el
campo "Ir a altura" de Home Assistant. Los guiones Python de viaje quedaron
retirados en [../tools/legacy/](../tools/legacy/) — compiten con la máquina de
estados del firmware (revisión del 2026-08-23, ronda 2).

⚠️ **El movimiento continuo solo con supervisión.** Un cuelgue durante el viaje
deja el escritorio moviéndose y **el watchdog no ayuda** —abre el canal, y abrir
no frena—. Detalle en [SEGURIDAD.md](SEGURIDAD.md).

**Paso 4 — Repetir para los otros tres.** ✅ **HECHO el 2026-08-22.**

Los cuatro verificados en el bus, a la primera:

| Canal | Pin | Botón | Código | Efecto observado |
|---|---|---|---|---|
| 1 | GPIO 27 | **Subir** | `0x47` | No movió con 300 ms |
| 2 | GPIO 26 | **Bajar** | `0x57` | Verificado el 2026-08-21 |
| 3 | GPIO 25 | **Memoria 80 cm** | `0x67` | Viajó 073 → 080 |
| 4 | GPIO 33 | **Memoria 117 cm** | `0x6F` | Viajó 080 → 117 |

**Los cuatro coinciden con lo predicho por [PROTOCOLO.md](PROTOCOLO.md)** desde
el 2026-08-06. El reset sigue sin cablear ([ADR-008](DECISIONS.md)).

Captura: [cuatro-canales-verificados](capturas/2026-08-22-cuatro-canales-verificados.log).

**La fase 3 queda cerrada.**

**Comprobación tras tocar el hardware:** `tools/verificar_canales.py` dispara los
cuatro y contrasta el código que ve la caja contra el esperado. Los canales de
memoria se frenan en cuanto delatan su código, así que no hay viajes largos y el
escritorio se queda donde estaba. **Requiere `PULSE_MS = 300`** para que subir y
bajar no muevan nada durante el barrido.

Pasado limpio el 2026-08-22 después de cerrar la tapa del mando: los cuatro OK,
94 cm antes y después.

Los **PC817 ya están comprobados** y no necesitan nada más: son chips sueltos en
DIP-4, sin jumper ni circuitería que neutralizar.

El pin de CLK ya está resuelto: **P18**, confirmado presente en la bornera
([ADR-020](DECISIONS.md)). Mapa completo de la bornera en
[HARDWARE.md](HARDWARE.md).

**El atajo del puerto de accesorios está descartado** (paso C, abajo): la caja de
control solo tiene la entrada de corriente, el conector del mando y el cable de
los motores. El bus del mando es el único camino.

### Estado físico del montaje

Todo montado y funcionando desde el 2026-08-06:

| | |
|---|---|
| Derivaciones | Tres hilos soldados al conector JST del mando, con el conector original puesto. El amarillo **no** se soldó |
| Sonda | En protoboard, 9.1 k + 7.4 k arriba y 27 kΩ abajo, por canal ([ADR-022](DECISIONS.md)) |
| Conexión | P18 = CLK (rojo), P4 = DIO (verde), GND común |
| ESP32 | Por USB al Mac, con la versión de **muestreo por ráfagas** de `desk_sniffer` cargada |
| Niveles | Bus 4.7 V, nodo del GPIO 2.9 V |
| El mando | **Funciona con normalidad** con todo conectado |

**Si hay que desconectar y volver:** USB primero, hilos del bus después
([ADR-019](DECISIONS.md)); al desmontar, al revés. Y el divisor solo se
comprueba **con el cable azul fuera**, o el mando aporta un camino paralelo de
~34 kΩ y el número no significa nada.

### Cómo capturar

El sniffer vuelca por serie a **115200**. *(Fue 921600, luego 460800, y acabó en
115200 el 2026-08-21: es la única velocidad en la que se ha comprobado de punta a
punta que se reciben comandos y se lee bien el bus. **El fallo de recepción no
está explicado** — [ADR-026](DECISIONS.md).)*
Para grabar a archivo sin pelearse con el Serial Monitor del IDE —que hay que
cerrar antes, porque solo un programa puede tener el puerto—:

```
exec 3</dev/cu.usbserial-0001
stty -f /dev/cu.usbserial-0001 115200 raw -echo
cat <&3 >> docs/capturas/AAAA-MM-DD-descripcion.log
```

⚠️ **Esto solo sirve para escuchar.** Para *mandar* comandos (`1`–`4`, `h`, `s`)
hace falta un solo descriptor de lectura y escritura con la velocidad fijada por
el ioctl nativo de macOS; con redirecciones de shell los bytes no llegan. Script
listo en [../tools/serial_talk.py](../tools/serial_talk.py).

El descriptor se abre **antes** de fijar la velocidad: si se hace al revés,
macOS reinicia la configuración al abrir el puerto y sale ilegible.

Cabecera de contexto obligatoria en cada captura, formato en
[capturas/README.md](capturas/README.md).

### Tareas sueltas, sin dependencias

- [x] **Cronometrar el umbral de grabar preset — 3.0 s.** Medido en el bus el
      2026-08-06, sin arriesgar ningún preset. Desbloquea el cableado de M1/M2
      ([ADR-010](DECISIONS.md))
- [x] **Instalar Arduino IDE, compilar y cargar el sniffer** — hecho el
      2026-08-03. Compila, arranca y la autocomprobación reporta bien
- [ ] Guardar las fotografías macro del chip y de la serigrafía en
      [hardware/fotografias/](hardware/fotografias/), que ya tiene el índice de
      cuáles faltan
- [x] **Cuál de las dos memorias es M1 y cuál M2** — resuelto el 2026-08-22.
      **M1 → canal 3 → `0x67` → 80 cm. M2 → canal 4 → `0x6F` → 117 cm.**

      *Dos fuentes distintas, y conviene no mezclarlas:* la relación
      **canal → altura** está **medida en el bus** —el canal 3 recorrió 073→080
      y el canal 4, 081→117—; la relación **botón → canal** la aporta quien
      soldó los cables, porque la etiqueta del mando no se ve desde el bus.
      Las dos coincidieron con lo que él predijo antes de probar
- [ ] Identificar los **cinco bytes de reposo**: `0x07`, `0x17`, `0x27`, `0x2E`,
      `0x2F`. **Ninguno lleva el bit `0x40`**, así que ninguno es una tecla
      pulsada y no estorban. Probablemente indican qué columna se escanea. En
      [cuatro-canales-verificados](capturas/2026-08-22-cuatro-canales-verificados.log)
- [ ] Identificar el byte de teclado **`0x27`**, visto el 2026-08-21. **No es una
      tecla pulsada** —le falta el bit `0x40`, así que el decodificador lo trata
      como reposo, igual que `0x17`— pero no se sabe en qué se diferencia de
      `0x17`. Aparece en
      [capturas/2026-08-21-canal2-verificado-0x57.log](capturas/2026-08-21-canal2-verificado-0x57.log)
      y alrededores
      físico es cuál. Trivial, y solo hace falta al cablear

<a id="decisiones-pendientes"></a>

### Decisiones pendientes

*Ninguna abierta ahora mismo.*

Cerradas el 2026-08-06:

- ~~Si subir y bajar se acotan por hardware~~ → **Sí, y con el mismo circuito
  que M1 y M2**: limitador de ancho de pulso a 300 ms en los cuatro canales
  ([ADR-023](DECISIONS.md)). La medición reveló que subir/bajar tienen dos
  regímenes y que el peligroso —el movimiento continuo— **no se puede parar
  abriendo un contacto**, solo evitar que arranque.

Cerradas el 2026-08-03:

- ~~Si se compra el analizador lógico~~ → **No.** La restricción de partida del
  proyecto se mantiene: el ESP32 hace de instrumento. Se compraría solo si la
  captura sale sucia y no se puede diagnosticar a ciegas. Ver
  [COMPRAS.md](COMPRAS.md).
- ~~Qué se usa para accionar~~ → **Ni photoMOS ni relés mecánicos de entrada:**
  PC817 primero, los relés del inventario si no encaja
  ([ADR-021](DECISIONS.md)). Los photoMOS cuestan ~$120.000 COP en Colombia y la
  premisa de coste de ADR-017 no se sostiene aquí.

Los pasos A, B y C de abajo se conservan como registro de cómo se llegó aquí.

### A. Identificar los 4 hilos por continuidad al chip ✅ HECHO

**Resultado: rojo = SCL, verde = SDA, azul = GND, amarillo = VDD (5 V).**
Verificado a 0.2 Ω el 2026-08-02. Ver [HARDWARE.md](HARDWARE.md).

El procedimiento original se conserva abajo por si hay que repetirlo, con el
pinout ya corregido: patas **2 = SCL, 3 = SDA, 4 = GND, 10 = VDD**.

<details>
<summary>Procedimiento (ya ejecutado)</summary>

Con el conector desenchufado de la caja de control, multímetro en **ohmios**
(no en continuidad — el pitido no atraviesa componentes en serie y su umbral
puede ser permisivo), medir cada hilo contra las patas del chip.

Numeración: pata 1 en la esquina del punto hundido, pata 16 la de enfrente.

Una lectura de ~0.2 Ω es pista directa. Cualquier cosa por encima de unos pocos
ohmios no es conexión, es otra cosa.

Los pines son de paso 1.27 mm. Punta fina y cuidado con puentear contiguos.

</details>

### B. Medir el pull-up del bus — YA NO HACE FALTA

**Resuelto por el datasheet, no por medición.** El pull-up es **interno al
chip**: 550 µA típicos, unos 9.1 kΩ a 5 V. Si además hubiera resistencias
externas, el pull-up solo sería más fuerte. Ver [ADR-013](DECISIONS.md).

La medición sigue siendo informativa —diría si existen los 10 kΩ externos del
circuito recomendado— pero no cambia el diseño en ninguno de los dos
resultados posibles. Opcional, por curiosidad.

<details>
<summary>Procedimiento (opcional)</summary>

Con el mando conectado a la caja de control y todo **desenchufado de la
corriente**, multímetro en ohmios:

| Entre | Se espera | Qué significa |
|---|---|---|
| Rojo (SCL) ↔ Amarillo (5 V) | 1–10 kΩ | El pull-up de la línea de reloj |
| Verde (SDA) ↔ Amarillo (5 V) | 1–10 kΩ | El pull-up de la línea de datos |
| Rojo ↔ Verde | Alta | Confirma que son dos señales independientes |
| Rojo ↔ Azul (GND) | Alta | Confirma que no hay pull-down |

Anotar los cuatro números en la bitácora aunque alguno salga raro —
especialmente si sale raro.

Conviene medir **dos veces**: con el mando conectado a la caja y con el mando
suelto. Si el valor solo aparece con el mando conectado, los pull-ups están
dentro de la caja de control; si aparece con el mando suelto, están en la placa
del mando.

</details>

### C. Mirar la caja de control por fuera ❌ DESCARTADO — no hay atajo

**Comprobado por inspección visual el 2026-08-03. No existe puerto de
accesorios.** Lo único que hay en la caja de control es:

1. La entrada del **adaptador de corriente** (29 V).
2. El conector del **mando**, 4 hilos.
3. El cable de **6 hilos** de los motores.

Nada más. **Ningún RJ11, RJ12 ni RJ45 de accesorios.**

El atajo que se buscaba —esas cajas Jiecang que llevan un puerto adicional para
Bluetooth o segundo mando, con protocolo serie 9600 8N1 ya documentado e
implementaciones ESPHome listas— **no aplica a este escritorio**. Era lo
esperable: un mando con AiP650E/TM1650 indica una caja de gama sencilla.

**Consecuencia: el plan sigue exactamente como está.** El bus del mando es el
único camino, y por eso mismo deja de haber una alternativa pendiente de
explorar. Referencias del atajo descartado en [REFERENCIAS.md](REFERENCIAS.md).

---

## Fase 1 — Reconocimiento ✅

Identificar el hardware y el punto de intervención.

- [x] Identificar el mando: `JK-CH506 Rev1.2`, Jiecang
- [x] Identificar el chip: AiP650EO, familia TM1650, sin MCU en el mando
- [x] Verificar el pinout del cable con multímetro
- [x] Confirmar que los pulsadores están a 5 V
- [x] Inventariar resistencias
- [x] Identificar la función de los 5 pulsadores: subir, bajar, M1, M2, reset
- [x] **Identificar los 4 hilos: rojo SCL, verde SDA, azul GND, amarillo 5 V.**
      Corrige el supuesto del handover, que daba el rojo como VCC
- [x] Averiguar cómo se **graba** un preset: manteniendo pulsado M1 o M2.
      Falta cronometrar el umbral, abajo

## Fase 2 — Sniffing ✅

Escuchar el bus sin perturbarlo y decodificar la altura.

- [x] Pull-up del bus: resuelto por datasheet, 9.1 kΩ internos
- [x] Diseñar la sonda: divisor 15 kΩ / 33 kΩ ([ADR-013](DECISIONS.md))
- [x] Medir con multímetro todo el cajón de resistencias — 30 piezas
- [x] Sonda final definida: 9.1 kΩ / 27 kΩ ([ADR-016](DECISIONS.md))
- [x] Criterio de verificación de la sonda corregido: se juzga por cociente, no
      por valor absoluto ([ADR-018](DECISIONS.md))
- [x] **Conseguir dos resistencias de 27 kΩ** — recibidas y medidas el
      2026-08-03, junto con las de 10 kΩ y 330 Ω de la fase 3
- [ ] **Soldar las derivaciones al conector JST del mando (AWG 28)** ← aquí estamos
- [x] CLK movido de P16 a **P18**, libre en WROOM y en WROVER: el módulo no se
      puede identificar con certeza y así deja de importar
      ([ADR-020](DECISIONS.md))
- [x] **P18 confirmado en la bornera**, y mapa completo de las dos columnas
      anotado en [HARDWARE.md](HARDWARE.md)
- [ ] **Verificar que el mando sigue funcionando con las derivaciones puestas,
      antes de conectar el ESP32.** Si algo cambió, el problema es de soldadura.
- [ ] Lectura de referencia del bus **sin la sonda**, para el cociente
- [ ] Montar la sonda en protoboard y **verificar el nivel del bus por cociente**
      (≈0.80 esperado; por debajo de 0.70, desconectar)
- [x] **Escribir el sniffer** — hecho, en
      [firmware/desk_sniffer/](../firmware/desk_sniffer/). Decodifica altura,
      teclas y control de display contra el datasheet
- [x] **Compilar y cargar el sniffer en el ESP32 real** — hecho el 2026-08-03.
      Compila, arranca, el serie a 921600 va y la autocomprobación de líneas
      reporta correctamente. Falta probarlo **con datos en el bus**
- [x] **Sonda montada y verificada** — 2026-08-06. Bus a 4.7 V, GPIO a 2.9 V,
      mando funcionando con normalidad. El pull-up real resultó ser de 2.4 kΩ y
      no de 9.1 kΩ, lo que obligó a rehacer el divisor
      ([ADR-022](DECISIONS.md))
- [x] **Primera captura del bus** — el sniffer lee comandos que coinciden con el
      datasheet: `48`, `6A`, `6C`, `6E`, `4F`
- [x] **Arreglar el encuadre.** El bus corre a ~202 kHz, por encima del techo de
      la captura por interrupción. Un histograma de intervalos descartó que
      fueran flancos contados dos veces. El sniffer pasa a **muestreo por
      ráfagas** a 4 MHz
- [x] Capturar tráfico con el escritorio quieto (refresco del display)
- [x] **Correlacionar bytes con el número visible en pantalla → altura
      decodificada.** Verificado en cuatro casos, incluido uno de tres dígitos
- [x] Capturar cada pulsador por separado. **Subir `0x47`, bajar `0x57`,
      memorias `0x6F` y `0x67`.** El reset no se pulsó ni se pulsará
- [x] **Comprobar que la altura se refresca durante el movimiento — SÍ.** Cada
      centímetro, ~1.2 s. **El lazo cerrado es viable**
- [x] **Comprobado si el chip llega a dormirse: NO.** 15 minutos de reposo,
      4505 armados y **ninguno con el bus en silencio**. El display se apaga
      escribiendo ceros, pero el refresco no se detiene nunca y el control
      siempre dice `sleep=no`. Acota [ADR-012](DECISIONS.md)
- [x] **Rango real medido: 73 a 118 cm.** Al topar no ocurre nada distinguible
      de estar parado
- [x] Documentar el protocolo en [PROTOCOLO.md](PROTOCOLO.md)

Medición aparte, sin instrumentos y sin riesgo, que puede hacerse cuando sea:

- [x] **Umbral de mantener M1/M2 hasta que graba: 3.0 s.** Medido en el bus el
      2026-08-06 ([ADR-010](DECISIONS.md) desbloqueado)
- [x] **Velocidad confirmada: 8.5 mm/s**, ~1.2 s por centímetro, con rampa de
      2.5–3 s en los primeros centímetros

### Sobre el sniffer

Escrito y revisado, en [firmware/desk_sniffer/](../firmware/desk_sniffer/), con
sus instrucciones en [firmware/README.md](../firmware/README.md).

Captura pasiva por interrupción en ambas líneas, decodificado fuera de la
interrupción, y volcado por serie con marcas de tiempo. No asume
direccionamiento I2C ([ADR-006](DECISIONS.md)).

Si al conectarlo se pierden flancos —lo dirá el contador de descartes con el
volcado crudo apagado— la alternativa robusta es capturar con el periférico RMT
o I2S en vez de por interrupción.

## Fase 3 — Accionar ✅

**Camino base: relés.** Leer altura por el bus, accionar por relés sobre cuatro
pulsadores — subir, bajar, M1, M2. El reset queda fuera del circuito
([ADR-008](DECISIONS.md)). Con la altura en lazo cerrado esto alcanza cualquier
altura, no solo los presets ([ADR-009](DECISIONS.md)).

- [ ] Verificar el estado de los GPIO de relé en arranque y reset **antes** de
      conectarlos ([ADR-010](DECISIONS.md))
- [ ] Pulsos de M1/M2 acotados por temporizador independiente
- [ ] Abortar movimiento ante cualquier lectura de altura incoherente

**Descartada: inyección en el bus.** Imposible eléctricamente —
[ADR-011](DECISIONS.md). El bus queda de solo lectura para siempre.

## Fase 4 — ESPHome / Home Assistant 🔶 en curso

Destino: HA sobre Ultron (Raspberry Pi 5). **Catálogo completo de lo que se va a
exponer —estado, uso, eventos, controles y diagnóstico— en
[INTEGRACION_HA.md](INTEGRACION_HA.md)**, abierto el 2026-08-22.

Aquí entran los límites de altura por software y las condiciones de movimiento
seguro. Ver [SEGURIDAD.md](SEGURIDAD.md).

### El riesgo que bloqueaba la fase, medido y descartado

**La radio WiFi nunca se había encendido en esta placa.** El sniffer muestrea a
4 MHz con las interrupciones apagadas 2 ms por ráfaga, y el stack de WiFi
necesita CPU: podían estropearse mutuamente, y esta placa **ya se colgó dos veces
el 2026-08-03** por saturación de interrupciones.

**Medido el 2026-08-22, dos corridas de 60 s idénticas salvo la radio:**

| | Sin WiFi | Con WiFi |
|---|---|---|
| Ráfagas | 299 | 298 |
| Transacciones | 1502 | 1499 |
| **Malformadas** | **0.67%** | **0.93%** |
| Muestras tardías | 15 | 17 |
| Reloj | 137 kHz | 137 kHz |

**La radio no degrada la captura.** La diferencia cae dentro del ruido de fondo
ya documentado (~0.8%). Coste: el programa pasa del 21% al 67% de la flash y la
RAM del 9% al 16%. Captura:
[wifi-impacto](capturas/2026-08-22-wifi-impacto.log). Sketch:
[../firmware/test_wifi_impact/](../firmware/test_wifi_impact/).

⚠️ **Supuesto, no verificado:** esto es **modo AP sin clientes**, el caso más
suave. **Falta medirlo en modo STA con tráfico real** antes de dar la
arquitectura por buena. Hace falta el SSID y la contraseña de la red.

### Orden de trabajo

1. ⚠️ **Los límites de altura por software, primero.** Es la condición que
   [ADR-028](DECISIONS.md) pone para quitar la supervisión del movimiento
   continuo. **Va antes que cualquier botón accesible desde el móvil**: hoy el
   freno depende de que el ESP32 siga vivo.
2. Repetir la medida de WiFi **en modo STA con tráfico**.
3. Elegir transporte —**MQTT con Discovery** es la recomendación, razonada en
   [INTEGRACION_HA.md](INTEGRACION_HA.md)— y publicar el estado.
4. Controles y presets con nombre.
5. Diagnóstico y estadísticas de uso.

### Presets propios por software — decidido el alcance el 2026-08-22

**Alturas con nombre, definidas en software, independientes de las dos memorias
del mando.** `{"de pie": 117, "sentado": 75, "reunión": 95}` y la altura se
alcanza con el control por altura que ya funciona.

**No hace falta nada nuevo.** Está demostrado: en la prueba de recorrido del
2026-08-22 el escritorio fue a **95 cm, que no es ninguna memoria del mando**.
Leer la altura (fase 2) + accionar (fase 3) + frenar por altura (probado) es todo
lo que se necesita.

**Ventaja sobre las memorias del mando:** son dos y sin nombre, y cambiarlas
exige una pulsación de 3 s con riesgo de sobrescribir la que había. Los presets
por software son ilimitados y se editan en un fichero. **Y no se pisan entre
sí:** el pulso largo está acotado a 2800 ms ([ADR-028](DECISIONS.md)), por debajo
de los 3.0 s que graban un preset, así que **el software no puede sobrescribir
una memoria del mando ni por error**.

**Límites, que son reales:**

- **Resolución de 1 cm.** El display da centímetros enteros y tras frenar quedan
  ~1 cm de inercia. Se anticipa y se ajusta con toques —los tres objetivos del
  2026-08-22 se clavaron— pero **por debajo del centímetro no hay información**.
- **El ESP32 debe sobrevivir al viaje.** Un cuelgue en movimiento continuo no lo
  para nadie ([ADR-028](DECISIONS.md)). **Con supervisión** hasta que existan los
  límites por software.
- **El display se duerme** por inactividad: hay que despertarlo con un toque
  antes de fiarse de la primera lectura.

**Paso 5 — Confiar en los recordatorios de extremo a extremo. ⬜ EN CURSO
desde el 2026-09-02.**

La fase 4 está montada y funcionando, pero **los recordatorios han fallado en
silencio tres veces** (2026-08-24, 08-31 y 09-02), siempre por motivos distintos
y **ninguno visible desde la interfaz de Home Assistant**. Los dos últimos —el
parpadeo del sensor de presencia y el contador que se borraba en cada reinicio—
están corregidos y verificados el 2026-09-02.

✅ **Primera verificación completa el 2026-09-02 a las 12:30**: avisó, esperó,
re-verificó y **subió de 80 a 117 sin intervención manual**. Detalle en la
[bitácora](BITACORA.md).

Falta **observar un par de días de uso real**: un ciclo bueno no descarta los
fallos intermitentes, que es exactamente lo que han sido los tres anteriores. Un
aviso que no acaba en movimiento es el fallo característico de este sistema.

⚠️ **Y hay un segundo asunto abierto, distinto:** el 2026-09-02 se dispararon
**nueve avisos** —07:15, 07:45, 08:20, 08:55, 09:30, 10:50, 11:25, 12:00 y
12:30— y el propietario **no vio ninguna notificación**. La notificación se
manda antes de la espera de 110 s, así que se enviaron. **No se pudo comprobar
si llegaron al móvil porque los logs de esa franja ya se habían rotado.**

**Al retomar, mirar esto primero**, y con los logs frescos: si el canal
`notify.mobile_app_icesar_pro` no entrega, el sistema puede mover el escritorio
sin avisar antes — que es peor que no moverlo.

**Cómo comprobarlo sin adivinar:** las consultas a la base de datos del
`recorder` están en [INTEGRACION_HA.md](INTEGRACION_HA.md). Esa sección existe
porque el 2026-09-02 emití diagnósticos plausibles y equivocados durante toda una
sesión teniendo 30 días de registros a mano sin mirarlos.

Suelto conocido, **sin explicar**: el sensor Zigbee revierte su retardo a 30 s
por su cuenta. No bloquea nada —la protección real es
`binary_sensor.escritorio_presencia_sostenida`— pero está sin entender.

**Paso 5b — Decidir sobre la ventana de 2800 ms.** El pulso largo no se puede
abortar y, si se pulsa la tecla contraria mientras dura, la caja ve SUBIR y
BAJAR a la vez. **Qué hace con eso no está verificado.** La comprobación es
barata —pulsar ambas en el mando, sin el ESP32— y decide si hace falta arreglo.
Detalle en [SEGURIDAD.md](SEGURIDAD.md). Si se decide usar M1/M2 para los
objetivos de postura, **hace falta un ADR**: contradice una decisión ya tomada.

**Paso 6 — La placa.** Especificación y planos listos en
[hardware/PCB_ESPECIFICACION.md](hardware/PCB_ESPECIFICACION.md). Dos cosas
**antes** de encargar nada:

1. **Identificar el conector del cable original** (paso, anclaje, marca). Sin eso
   no se puede elegir el JST.
2. **Probar el buffer 74HC14 en protoboard.** Va en la placa y nunca se ha
   montado.

**Paso 7 — Capa 1 de protección eléctrica.** El cargador del ESP32 y el
escritorio en la misma regleta. Cuesta cero y sigue pendiente
([SEGURIDAD.md](SEGURIDAD.md)).

## Fase 5 — App ⬜

Frontend propio contra la API WebSocket de HA. Estética HUD tipo JARVIS (cyan,
JetBrains Mono), consistente con Aitri Hub.

Métricas: horas sentado vs. de pie, recordatorio de cambio de postura, subir
automáticamente al arrancar la primera reunión de la tarde.
