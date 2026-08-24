# Integración con Home Assistant

> Qué expone el escritorio a HA y por qué. Diseño de la fase 4, abierto el
> 2026-08-22. Las decisiones firmes van a [DECISIONS.md](DECISIONS.md); esto es
> el catálogo y el razonamiento.

Destino: **HA sobre Ultron (Raspberry Pi 5)**.

---

## El principio que ordena todo lo demás

**El ESP32 publica hechos. Home Assistant deriva estadísticas.**

El ESP32 publica lo que *observa* —altura, teclas, salud del bus— y HA se encarga
de acumular, historizar y graficar. Los motivos:

- **HA ya tiene base de datos, gráficas y estadísticas de largo plazo.**
  Reimplementar eso en un microcontrolador es trabajo tirado.
- **Lo que acumule el ESP32 se pierde al reiniciar.** "Tiempo de pie hoy"
  guardado en RAM vuelve a cero con cada corte de USB.
- **El firmware se mantiene pequeño**, y el firmware pequeño es el que sigue
  funcionando. El sniffer es código con timing crítico; cuanto menos cargue,
  mejor.

**Excepción:** los contadores que necesitan muestreo más rápido de lo que se
publica —la distancia total recorrida, que cambia a 0.68 cm/s— se acumulan en el
ESP32 y se publican ya sumados.

---

## Lo que ya funciona — 2026-08-22

**Trece entidades, creadas solas por discovery.** Verificado de punta a punta:
Home Assistant → MQTT → ESP32 → optoacoplador → mando → caja de control, con la
confirmación volviendo por el bus.

| Tipo | Entidades |
|---|---|
| Sensores | altura, antigüedad de la altura, bus malformadas, bus transacciones, uptime, WiFi RSSI |
| Binario | display despierto, **online** (testamento MQTT: el broker lo pone en `off` si el ESP32 muere, sin depender del firmware) |
| Botones | subir, bajar, memoria 1, memoria 2, **parar**, refrescar altura |

⚠️ **Solo se exponen toques, nunca el pulso largo.** Nada pulsable desde un móvil
puede arrancar movimiento continuo (2.2 s) ni sobrescribir un preset (3.0 s). Es
una decisión, no una omisión: el movimiento continuo sigue necesitando
supervisión ([ADR-028](DECISIONS.md)).

⚠️ **Lo que sí puede pasar desde el móvil: pulsar M1 o M2 arranca un viaje de
hasta 44 cm**, porque lo ejecuta la caja de control por su cuenta. El botón
**parar** está en la misma pantalla precisamente por eso.

Captura: [controles-mqtt](capturas/2026-08-22-controles-mqtt.log).

**Falta:** el `number` para ir a una altura concreta, los presets con nombre y
las estadísticas de uso — y antes que todo eso, **los límites por software**.

**Desde el 2026-08-23 el ESP32 corre en un cargador de pared**, independiente
del Mac. El puerto serie deja de estar disponible salvo que se reconecte al Mac
(y **reflashear requiere el Mac**: no hay OTA todavía). El monitoreo y los
comandos van por MQTT.

⚠️ **Al cambiar la fuente de alimentación del ESP32**, el orden de siempre:
escritorio desenchufado → cambiar el USB → esperar el arranque → escritorio a la
corriente. El instante sin alimentación con los hilos puestos es la condición de
[ADR-019](DECISIONS.md)/[ADR-031](DECISIONS.md).

### Monitoreo automático — instalado el 2026-08-23

Cinco automatizaciones en HA (en `automations.yaml`, editables desde la
interfaz), notificando al móvil **iCesar pro**:

| Automatización | Cuándo avisa |
|---|---|
| ALERTA desconectado | El ESP32 lleva 2 min sin publicar (lo detecta **el broker**, no el firmware) |
| Volvió | Al reconectar |
| ALERTA bus degradado | Malformadas >2% sostenido 10 min — la sonda degradándose |
| 🚨 Movimiento sostenido | Subiendo o bajando >3 min — ningún viaje legítimo dura tanto (el recorrido entero son ~65 s) |
| Resumen cada 30 min | "Escritorio OK — altura, bus, WiFi, movimiento". **Solo si está online**; si cansa, se desactiva desde la interfaz y quedan solo las alertas |

**Los logs ya se guardan solos**: el recorder de HA conserva el historial de
todas las entidades (por defecto ~10 días), consultable en el panel de historial.
Para estudio de largo plazo está la integración InfluxDB ya presente — hoy
configurada sin entidades incluidas; si se quiere histórico permanente del
escritorio, se añaden ahí.

**Nota**: `altura` publica `unknown` cuando el display duerme; el discovery lleva
`value_template` para que HA lo trate como estado desconocido y no como error.

## Estado del escritorio

Lo que hace falta para saber qué está pasando ahora mismo.

| Entidad | Tipo | Origen | Estado |
|---|---|---|---|
| `altura` | sensor, cm | Display del bus | ✅ **Ya se mide** |
| `estado` | sensor: `quieto`/`subiendo`/`bajando` | Evolución de la altura + tecla vista | 🔶 Derivar |
| `altura_objetivo` | sensor, cm | Del controlador, cuando hay viaje | 🔶 Implementar |
| `display_despierto` | binary_sensor | Los 4 dígitos a `0x00` = dormido | ✅ **Ya se mide** |
| `antiguedad_altura` | sensor, s | Segundos desde la última lectura válida | 🔶 Derivar |

⚠️ **`antiguedad_altura` no es un adorno de diagnóstico: es seguridad.**
[ADR-012](DECISIONS.md) dice que con altura obsoleta no se inicia movimiento, y
esta es la entidad que lo hace comprobable desde HA. **Si el display lleva
dormido un rato, la altura que se muestra es la última conocida, no la actual.**

---

## Uso — lo que hace la integración interesante

Aquí es donde deja de ser un mando a distancia y pasa a ser algo que sabe cosas.

| Entidad | Tipo | Cómo sale |
|---|---|---|
| `tiempo_de_pie_hoy` | sensor, min | HA acumula sobre `altura` y un umbral |
| `tiempo_sentado_hoy` | sensor, min | Ídem |
| `cambios_de_altura_hoy` | contador | HA, sobre eventos de movimiento |
| `altura_min_hoy` / `altura_max_hoy` | sensor, cm | HA, estadísticas del día |
| `ultimo_movimiento` | timestamp | HA |
| `distancia_recorrida_total` | sensor, m | **ESP32**, acumulando \|Δaltura\| |
| `pulsaciones_por_boton` | 4 contadores | ESP32 o HA, sobre los eventos de tecla |

**El umbral de pie/sentado hay que elegirlo**, y no hay un valor universal:
depende de la estatura. El rango físico es 73–118 cm. Queda como
[decisión pendiente](#decisiones-pendientes).

**`distancia_recorrida_total` es el indicador de desgaste del motor.** Es el único
número que dirá algo cuando el escritorio empiece a fallar dentro de unos años.

---

## Eventos — lo que más juego da

**El sniffer ve las teclas del mando físico, no solo las que manda el ESP32.**
Eso significa que HA puede enterarse de que **una persona ha tocado el mando**, y
eso abre automatizaciones que de otro modo no existen.

| Evento | Cuándo | Para qué sirve |
|---|---|---|
| `boton_pulsado` | Cualquier tecla en el bus, con su código | Distinguir persona de automatización |
| **`uso_manual`** ✅ | Segundos desde la última tecla humana en el mando | **Implementado 2026-08-23.** La automatización que no quiere pelearse con la persona consulta esto |
| `movimiento_no_pedido` ✅ | El escritorio se mueve sin que el ESP32 lo pidiera | **Implementado 2026-08-23 como cesión automática**: una tecla no ordenada por el ESP32 cancela su viaje sin frenar (la persona ya frenó). Verificado en vivo: `ultimo_freno: mando manual` |
| `preset_recuperado` | `0x67` o `0x6F` en el bus | Saber a qué altura se va antes de llegar |
| `tope_alcanzado` | La altura deja de cambiar en 73 o 118 | Fin de recorrido |

**Verificado**: los cuatro códigos de tecla se leen del bus y se distinguen sin
ambigüedad —`0x47` subir, `0x57` bajar, `0x67` M1/80 cm, `0x6F` M2/117 cm— y el
sniffer los ve **tanto si los provoca el ESP32 como una persona**.

⚠️ **Lo que sí hay que resolver: distinguir quién pulsó.** El ESP32 sabe cuándo
ha sido él porque acaba de mandarlo; cualquier otra tecla es humana. La lógica ya
existe en `desk_sniffer` para atribuir un código a un canal — se extiende para
marcar el resto como manuales.

---

## Arranque y recuperación de estado

**Medido el 2026-08-22, incluso cortando la corriente con el escritorio en
marcha:** conserva la altura, conserva las memorias, **no reanuda el movimiento**
al volver la luz, y el bus revive solo. Lo que no sobrevive es el conocimiento
del ESP32: el display **arranca apagado**, y con el display apagado **la altura
no está en el bus**.

**El refresco no tiene coste:** trece toques de 300 ms medidos, cero deriva. Se
puede usar tan a menudo como haga falta.

**Secuencia de arranque, y el orden importa:**

1. Publicar la última altura conocida, **marcada como no confirmada**, con
   `antiguedad_altura` alto. Requiere guardarla en la flash del ESP32 (NVS), no
   solo en RAM.
2. Dar el **toque de refresco** (`w`, 300 ms): despierta el display **sin mover
   el escritorio** — las dos mitades están verificadas.
3. Publicar la altura real y poner `antiguedad_altura` a cero.

⚠️ **El paso 1 no se puede saltar, y el 3 tampoco.** El caso feo es este: vuelve
la luz, HA muestra 95 cm porque es lo último que vio, y resulta que alguien movió
el escritorio a mano mientras no había corriente. **Una altura vieja presentada
como actual es peor que no tener altura.**

⚠️ **El refresco nunca sobre un canal de memoria.** Cualquier toque en M1 o M2
arranca un viaje al preset. El comando `w` usa el canal de bajar, por eso.

## El botón de reset — lo que no sabemos

El mando tiene un botón de reset que **no está cableado a propósito**
([ADR-008](DECISIONS.md)): baja hasta el tope inferior, sin confirmación y sin
poder interrumpirse.

**Que no esté cableado no significa que no pueda pulsarlo una persona.** Y ahí
hay dos incógnitas que afectan a la integración:

- **El ESP32 lo vería** como movimiento que él no pidió — el evento
  `movimiento_no_pedido` de este catálogo lo cubre.
- ⚠️ **No se sabe si el reset altera las memorias.** Si recalibra el cero, las
  alturas de M1 y M2 podrían dejar de significar lo que significaban. **Supuesto,
  no verificado**, y comprobarlo cuesta un recorrido completo del escritorio.

  **Por eso [ADR-029](DECISIONS.md): el sistema no asocia M1 y M2 a ninguna
  altura.** Si nunca afirma que M1 vale 80, no puede mentir cuando deje de
  valerlo. Los presets por software **no dependen de las memorias del mando**, así
  que tampoco quedarían desplazados.

**Mientras no se compruebe:** si HA detecta un descenso largo no pedido que
termina en el tope inferior, lo prudente es **marcar la altura como no fiable** y
pedir confirmación antes de volver a usar presets.

## Controles

| Entidad | Tipo | Notas |
|---|---|---|
| `ir_a_altura` | number, 73–118 | Lazo cerrado, ya probado el 2026-08-22 |
| `subir` / `bajar` | button | Toque de 800 ms ([ADR-027](DECISIONS.md)) |
| `parar` | button | Toque en cualquier canal: **es el freno** |
| `preset` | select | Alturas con nombre, por software |
| `permitir_movimiento` | switch | Bloqueo maestro |
| `M1` / `M2` | button | Botones **opacos**: sin altura asociada ([ADR-029](DECISIONS.md)) |
| `m1_altura_observada` | sensor + fecha | A dónde llevó **la última vez**. Observación, no configuración |

⚠️ **`parar` no es opcional.** Con movimiento continuo, un toque es lo único que
detiene el escritorio ([ADR-028](DECISIONS.md)). Tiene que estar en cualquier
interfaz que pueda arrancar un viaje.

---

## Diagnóstico — salud del enlace

Sin esto, cuando algo falle dentro de tres meses no habrá forma de saber si es el
bus, la radio o el firmware. Y esta sesión ya demostró lo caro que sale no poder
distinguirlo.

| Entidad | Origen | Ya disponible |
|---|---|---|
| `bus_transacciones_s` | Estadísticas del sniffer | ✅ |
| `bus_malformadas_pct` | Ídem — **el indicador de salud de la captura** | ✅ |
| `bus_reloj_khz` | Ídem | ✅ |
| `muestras_tardias` | Ídem | ✅ |
| `wifi_rssi` | ESP32 | 🔶 |
| `uptime` / `motivo_ultimo_reinicio` | ESP32 | 🔶 |
| `memoria_libre` | ESP32 | 🔶 |

**Referencia medida el 2026-08-22, bus en reposo, para saber qué es normal:**
299 ráfagas y 1502 transacciones por minuto, **0.67% malformadas**, 137 kHz.
Con la radio encendida: 0.93%. Ver
[capturas/2026-08-22-wifi-impacto.log](capturas/2026-08-22-wifi-impacto.log).

---

## Automatizaciones que esto habilita

No es la lista de deseos: es lo que sale directamente del catálogo de arriba.

- **Recordatorio de postura.** Dos horas sentado → avisar. Con
  `tiempo_sentado_hoy` es una automatización de cuatro líneas.
- **Subir al empezar la jornada**, pero **solo si nadie ha tocado el mando en la
  última hora** — `movimiento_no_pedido` evita pelearse con la persona.
- **Bajar al detectar que no hay nadie**, encadenado a un sensor de presencia.
- **Aviso de bus degradado**: si `bus_malformadas_pct` se dispara sobre el 1%,
  algo va mal en la sonda. Se entera uno antes de que falle del todo.
- **Gráfica de altura del día**, que es lo que convierte esto en datos de salud
  y no en un juguete.

---

## Recordatorios de postura — instalado el 2026-08-23

**Solo avisan si estás delante.** Sensor de presencia `SNZB-06P` (mmWave: detecta
aunque estés quieto, no solo movimiento).

| Automatización | Dispara | Acción |
|---|---|---|
| Llevas mucho sentado | **45 min** con postura `sentado` **y presencia** | Notificación con botón *"Subir a 117"* |
| Llevas mucho de pie | **30 min** con postura `de pie` **y presencia** | Notificación con botón *"Bajar a 80"* |

⚠️ **No mueven el escritorio por su cuenta**: ofrecen hacerlo y esperan a que lo
pulses. Es lo que exige [SEGURIDAD.md](SEGURIDAD.md) —ningún movimiento
automático sin confirmación— y además evita que se mueva mientras trabajas.

### El sensor derivado que hizo falta, y por qué

`sensor.escritorio_jiecang_altura` publica **`unknown` cuando el display duerme**
(con el display apagado, la altura no está en el bus). Medir "cuánto llevo en
esta postura" con él es imposible: el dato desaparece cada pocos minutos.

Por eso hay dos sensores derivados en `configuration.yaml`:

- **`sensor.escritorio_altura_estable`** — conserva la última altura real
- **`sensor.escritorio_postura`** — `sentado` / `de pie`, con el **umbral en
  95 cm**. Es el número a tocar si no encaja con tu postura

**Las alturas de trabajo son 80 (sentado) y 117 (de pie)**, indicadas por el
propietario. El umbral de 95 las separa con holgura por los dos lados.

⚠️ **Trampa que costó un rato:** el disparador del template escrito con la
sintaxis nueva (`- trigger: state`) dentro de la clave antigua (`trigger:`)
**pasa la validación de HA y no dispara nunca**. Los sensores se quedan en
`unknown` sin un solo error en el log. Hay que usar `- platform: state`.

## Panel

En la sección **Estudio** del panel `Casa`: el campo **Ir a altura** (cm) y el
**indicador de movimiento**. Solo eso, a propósito — el resto de entidades vive
en la página del dispositivo, sin meter ruido en el panel diario.

## Decisiones pendientes

Ninguna se puede cerrar sin información que no está en el repositorio.

1. ~~**Transporte: MQTT o ESPHome.**~~ **Cerrado el 2026-08-22:
   [ADR-030](DECISIONS.md) fija MQTT.** El motivo decisivo es que el sniffer
   bloquea hasta 2.8 s por pulso y ESPHome exige componentes que devuelvan
   enseguida: portarlo obligaría a reescribir el código crítico.

   **Broker ya funcionando**, levantado y verificado ese mismo día:
   `ultron:1883`, usuario `esp32`, contraseña en Ultron en
   `~/mosquitto/config/.pass-esp32`.
2. **Umbral de pie/sentado**, en cm. Depende de la estatura.
3. **Credenciales WiFi**, para repetir la medición de impacto **en modo STA con
   tráfico real** — lo medido hasta ahora es AP sin clientes, el caso más suave.
4. **Los límites por software**, que es la condición que
   [ADR-028](DECISIONS.md) pone para quitar la supervisión del movimiento
   continuo. **Esto va antes que cualquier botón accesible desde el móvil.**
