# Bitácora

> Diario de sesiones. Lo más reciente arriba. Una entrada por sesión de trabajo,
> aunque no haya funcionado nada. Plantilla en
> [plantillas/entrada-bitacora.md](plantillas/entrada-bitacora.md).

---

## 2026-08-28 — Los contadores de postura contaban viajes a medias

> *"creo que los contadores de tiempo de cuando estoy parado o sentado no son
> precisos. Uno de los puntos es si pauso una actividad de subir o bajar, igual
> cuenta como si hubiera subido o bajado."*

**Cierto.** La postura se derivaba de **un umbral único en 95 cm**, y eso tenía
dos consecuencias que se propagaban hasta las estadísticas del día:

1. **La postura cambiaba a mitad de viaje.** Subiendo de 80 a 117, al cruzar los
   95 ya contaba "de pie" — medio minuto antes de llegar
2. **Un viaje interrumpido mentía indefinidamente.** Parar en 96 dejaba la
   postura en "de pie", y `history_stats` sumaba esas horas como tiempo de pie
   real

### La solución la propuso el propietario, y es mejor que la mía

Yo propuse una zona muerta (90–100 mantiene la postura anterior). Él propuso
algo más simple:

> *"cuando llegue a los targets 117 o 80, ahí sí marque con el tiempo"*

**Solo cuenta ESTAR en una altura de trabajo:**

| Altura | Postura |
|---|---|
| 77–83 | `sentado` |
| 114–120 | `de pie` |
| cualquier otra | `intermedia` — no cuenta |

Elimina el problema de raíz: un viaje interrumpido a 96 no es ninguna postura,
así que no arranca ningún contador ni ensucia ninguna estadística. Los ±3 cm
cubren ajustes a mano.

### La auditoría encontró más de lo buscado

Revisando la cadena completa aparecieron **eslabones que no estaban
documentados**: `escritorio_presencia_sostenida` (presencia con `delay_off` de
15 min) y `escritorio_postura_efectiva` (que marca `ausente` cuando no hay
nadie, para que **el escritorio olvidado arriba de noche no sume horas de pie**),
más cuatro contadores `history_stats`. Están bien resueltos; quedan recogidos en
[INTEGRACION_HA.md](INTEGRACION_HA.md) con el resto de la cadena.

**Y `uso_manual` sigue sin existir como entidad**: el firmware solo lo publica
tras el primer uso del mando desde el arranque. La condición que lo consulta ya
está blindada para no bloquear, pero **la cortesía de "no muevas si acabo de
usar el mando" no está protegiendo nada mientras el sensor no aparezca**.

### Nota de acceso

Durante esta sesión `ultron` dejó de resolver: **Tailscale estaba parado**. La
máquina estaba perfectamente —Home Assistant y el broker respondían— y se llegó
a ella por su IP local, `192.168.1.29`. Conviene recordarlo: el nombre depende
de Tailscale, la IP no.

---

## 2026-08-24 (noche) — El sensor de movimiento estaba muerto, y con él una protección

> *"la entidad que indica si está quieto o en movimiento, no funciona"*

**Cierto, y era una regresión mía de la segunda ronda adversarial.**

Para que la radio no se pusiera nunca delante de una decisión de freno, hice que
**durante el movimiento no se publicara nada** por MQTT (hallazgo F7). Correcto
en su intención — pero el estado de movimiento se publicaba **dentro** de esa
función, así que Home Assistant nunca llegaba a ver `subiendo` ni `bajando`:
solo `quieto`, antes y después.

⚠️ **Y lo grave no era el sensor.** La automatización *"parar si desaparece la
presencia mientras se mueve"* —la **capa 3**, la que cierra el caso de "me puedo
ir justo en la confirmación"— **comprueba ese mismo sensor**. Como nunca decía
`subiendo`, **esa protección no podía dispararse jamás desde que se creó.**

**Arreglado**: el estado de movimiento se publica **en cuanto cambia**, no en el
ciclo periódico. Una publicación pequeña por transición, que no compite con el
freno.

**Verificado en un viaje real**, secuencia vista por HA:

```
quieto → subiendo → frenando → quieto
```

### Y una segunda mitad, también señalada por el propietario

> *"si le pulso el mando no lee eso"*

Cierto: `g_motion` solo refleja **los viajes que ordena el ESP32**. Con el mando,
la caja mueve el escritorio y el firmware no tenía ningún estado que cambiar —
aunque **sí ve la altura cambiar en el bus**.

**Ahora se deduce:** si la altura cambia mientras el ESP32 está en reposo, es que
alguien lo está moviendo. El sensor publica **`subiendo (mando)` / `bajando
(mando)`**, distinguiendo quién mueve el escritorio. Vuelve a `quieto` a los 4 s
sin cambios de altura.

**Verificado en vivo**, con el mando en la mano:

```
altura 86 → subiendo (mando)
altura 90 → subiendo (mando)
altura 80 → bajando (mando)
altura 80 → quieto
```

**Beneficio de seguridad**: la automatización que para el escritorio si
desaparece la presencia ahora cubre también **un movimiento continuo arrancado a
mano y dejado corriendo**, que antes le era invisible.

### Lo que enseña

Un arreglo de seguridad —no publicar durante el viaje— **desactivó otra medida
de seguridad** que dependía de esos mismos datos. Ninguna de las dos rondas de
revisión lo vio, porque cada una miró el firmware y las automatizaciones por
separado: **el fallo vivía en la costura entre ambos.**

Y la segunda mitad —el mando invisible— llevaba ahí **desde que se creó el
sensor**, sin que ninguna revisión la señalara: el firmware reportaba
correctamente lo que sabía, y lo que no sabía no se lo preguntó nadie.

**Las dos las encontró el propietario usando el sistema.**

---

## 2026-08-24 (tarde) — El contador medía el mueble, no a la persona

Caso de uso planteado por el propietario:

> *"me voy al baño y no estoy, no sube. Cuando vuelva, ¿vuelve a contar de cero
> o entiende que recién volví? ¿y si vuelvo en una hora?"*

**El diseño lo hacía mal, y de una forma que no se ve hasta que se piensa el
caso.** El contador era `postura.last_changed`, o sea **"cuánto lleva el
escritorio a esta altura"**. Irse una hora no lo cambiaba: al volver el sistema
afirmaba "llevas hora y media sentado" y subía la mesa **justo cuando acababas
de sentarte**.

**Medía el mueble en vez de a la persona**, y las dos cosas coinciden solo
mientras nadie se levanta.

**Arreglado con un `input_datetime` que marca el inicio del periodo**, movido por
dos automatizaciones nuevas:

| Situación | Comportamiento |
|---|---|
| Cambio de postura | Periodo nuevo |
| **Ausencia < 15 min** | **No reinicia** — un baño no es un descanso |
| **Ausencia ≥ 15 min** | **Reinicia** y avisa cuántos minutos estuviste fuera |

**Los 15 minutos separan un recado de una pausa real.** Es un número elegido, no
medido: se ajusta en `escritorio_periodo_vuelta_larga` si la experiencia dice
otra cosa.

---

## 2026-08-24 — Los recordatorios no disparaban: dos fallos, ninguno visible

> *"hasta ahora no veo que la automatizacion funcione, si quedó activa?"*

Estaban activas y **no habían disparado nunca**. Dos fallos independientes, los
dos míos, y **ninguno dejaba rastro en el log**:

**1. El disparador esperaba una transición que no ocurre.** Escrito como *"cuando
la postura CAMBIE a sentado y siga 45 min"*. Si el escritorio ya estaba a 80 cm
desde el día anterior, la postura **ya era** `sentado`: no hay cambio que
capturar y no salta jamás. Corregido: se revisa **cada 5 min cuánto lleva** en la
postura, que es lo que se quería medir.

**2. Una condición contra un sensor inexistente.** La cortesía *"no muevas si se
tocó el mando en 5 min"* usa `uso_manual` — **que el firmware solo publica
después de que alguien toque el mando por primera vez**. Como no se había tocado
desde el último flasheo, el sensor no existía, la condición no podía evaluarse, y
**bloqueaba la automatización entera**.

**El segundo es el patrón que más vale registrar:** una condición de seguridad
que, al no poder evaluarse, **impide funcionar en vez de dejar pasar**. Es el
mismo error de forma que el ADR-019 —una salvaguarda razonando sobre un caso y
callando en otro— y aquí volvió disfrazado de plantilla YAML.

**Verificado**: se bajó el umbral a 60 s dejando solo la notificación (sin mover
el escritorio), llegó al móvil, y se restauraron 45/30 min con el movimiento.

**Anotado**: `last_changed` se reinicia con cada reinicio de HA, así que el
contador de postura vuelve a cero. Irrelevante en operación normal, pero durante
una sesión de cambios da la impresión de que nunca dispara — y eso fue
exactamente lo que pasó: al revisarlo, a la automatización **le faltaba un
minuto**, no estaba rota.

### ✅ VERIFICADO DE PUNTA A PUNTA — 2026-08-24

**La cadena completa funcionó sola, sin intervención:**

```
45 min sentado  ->  aviso al movil  ->  2 min de margen
                ->  re-verifica presencia  ->  SUBE A 117
                ->  aviso con boton "Dejalo en 80"
```

Es el primer momento en que el sistema hace algo **útil por su cuenta**: mide la
postura leyendo el bus, comprueba que hay alguien delante, espera por si te
vas, mueve el escritorio y ofrece deshacerlo. Ninguna de esas cosas la da una
alarma.

**Fase 4 cerrada en la práctica.**

---

## 2026-08-23 (final) — Recordatorios de postura con presencia

Lo primero del proyecto que da una utilidad que una alarma no da: **avisa solo
si estás delante**. Sensor `SNZB-06P` (mmWave, detecta presencia aunque estés
quieto).

| Automatización | Dispara | Acción |
|---|---|---|
| Mucho sentado | **45 min** sentado **+ presencia** | Aviso con botón *"Subir a 117"* |
| Mucho de pie | **30 min** de pie **+ presencia** | Aviso con botón *"Bajar a 80"* |

**Corregido el mismo día, y por dos preguntas del propietario que dieron en el
clavo:**

> *"si solo avisa para moverse, ¿qué sentido tiene el ESP?"*

Ninguno: eso lo hace una alarma. La primera versión solo notificaba con un botón
—precaución mía de más— y **vaciaba el sistema de su única utilidad**. Al releer
[SEGURIDAD.md](SEGURIDAD.md), lo que pide es *"confirmación de que hay alguien
delante"*, **y el sensor de presencia ES esa confirmación**. Ahora **mueven el
escritorio** y avisan, con un botón para deshacer.

Tercera condición añadida, que es la cortesía del sistema: **no mueve si se tocó
el mando en los últimos 5 minutos** (`uso_manual`, el sensor que salió de la
cesión al mando manual). Si acabas de ponerlo donde quieres, no te lo cambia.

**Y una tercera corrección, también del propietario:**

> *"creo que tiene como dos minutos, podría creer que aún estoy y levantar"*

**Cierto, y era un agujero de seguridad.** El sensor tiene **90 s de retardo
ocupado→desocupado**: en esa ventana informa "presente" con la silla ya vacía, y
la automatización habría movido el escritorio sin nadie delante — exactamente lo
que [SEGURIDAD.md](SEGURIDAD.md) prohíbe.

Se añadió una re-verificación tras 110 s. **Y el propietario volvió a tener
razón:**

> *"da igual, por que me puedo ir justo en la confirmacion"*

**Cierto: una comprobación puntual reduce la ventana, no la cierra.** Quedaron
tres capas:

| Capa | Qué hace |
|---|---|
| **1. Retardo del sensor: 90 s → 30 s** | Admite la ausencia tres veces antes |
| **2. Re-verificar tras 110 s** | Avisa, espera más que el retardo, vuelve a preguntar |
| **3. Parar si la presencia cae DURANTE el movimiento** | Vigilancia continua, no predicción |

**La capa 3 es la que cierra el caso.** Las otras dos adivinan; esta reacciona.

⚠️ **Y un error mío que hay que registrar:** dije que bajar el retardo del sensor
rompería el contador de postura. **Es falso.** El contador mide la **altura**
(`sensor.escritorio_postura`), no la presencia — la presencia solo se consulta
en el instante del disparo. Bajarlo no rompía nada, y estuve a punto de
descartar por ese razonamiento equivocado la mejora más simple de las tres.

> *"eso que la refresque, ¿es que va a enviar un toque cada rato?"*

Sí, y era mala idea. Se había puesto que el resumen despertara el display cada
30 min para dar una altura fresca: **desgaste del pulsador y luz encendida para
leer un número que casi nunca cambia**. Retirado. No hace falta — cuando alguien
mueve el escritorio, el display se enciende solo y el sensor estable se
actualiza.

**Alturas de trabajo: 80 sentado, 117 de pie**, indicadas por el propietario.
Umbral de postura en 95 cm.

### Hizo falta un sensor derivado, y la razón importa

`altura` publica **`unknown` cuando el display duerme** —con el display apagado
la altura no está en el bus—, así que **no sirve para medir tiempo en una
postura**: el dato desaparece cada pocos minutos. Se añadieron dos sensores
plantilla en `configuration.yaml`: `escritorio_altura_estable` (conserva la
última lectura real) y `escritorio_postura`.

⚠️ **Trampa que costó un rato, y vale para todo el proyecto:** el disparador del
template escrito con la sintaxis nueva (`- trigger: state`) dentro de la clave
antigua (`trigger:`) **pasa la validación de HA y no dispara nunca**. Los
sensores se quedaron en `unknown` **sin un solo error en el log**. La validación
que pasa no demuestra que funcione — solo la comprobación en vivo lo hizo.

### Calibración del sensor de presencia: sin tocar, y con motivo

El retardo ocupado→desocupado está en **90 s**, y **conviene dejarlo así para
esto**: si se acorta, cada ida a por agua marca ausencia y el contador de
"tiempo sentado" vuelve a cero — los avisos no llegarían nunca. Si 90 s resulta
largo para otra cosa (luces), lo correcto es que cada automatización ponga su
propia espera, no bajar el retardo del sensor.

---

## 2026-08-23 (cierre real) — El LED, y una pregunta que merece quedar escrita

### El LED no se puede apagar, y la entidad se retiró

Se implementó un latido en GPIO2 con interruptor en HA. **No funcionó, y la
causa no tiene arreglo por software: la única luz de esta placa es un LED ROJO
DE ALIMENTACIÓN**, cableado directo a la línea de 3.3 V. **Ningún GPIO llega
hasta él.** Verificado: el firmware aceptaba el comando y publicaba `ON`
mientras la luz seguía igual.

**Se retiró la entidad de HA** —un interruptor que visiblemente no hace nada es
peor que no tenerlo— y se limpiaron sus mensajes retenidos del broker. El código
del latido se queda, correcto y sin coste, accesible con la tecla `L` por serie:
cualquier DevKit con el LED azul habitual en GPIO2 sí lo tendría.

**Para apagarlo de verdad:** cinta aislante, o desoldarlo. Con el historial de
soldaduras de esta semana, la cinta.

**Hallazgo lateral:** la entidad salió como `switch.escritorio_led_2` porque ya
existía un `switch.escritorio_led` **de TP-Link** en la instalación. No era un
duplicado nuestro.

### La pregunta del propietario al cerrar

> *"no se para que hice esto si el boton lo tengo a 30 centimetros de alcance,
> pude solo crear una alarma en HA y no arriesgarlo asi"*

**Y tiene razón.** Queda escrito porque es la valoración más honesta del
proyecto que hay en toda esta bitácora:

- Para **subir y bajar** un escritorio con el mando al alcance, nada de esto
  hacía falta
- Para **recordar cambiar de postura**, una alarma en HA son cinco minutos y
  cero riesgo
- El coste real fueron tres días, y **estuvo a punto de costar el mando**

Lo que sí aporta y una alarma no: **saber** —no preguntar— si se está de pie o
sentado, **actuar** cediendo el paso a la persona, y tener histórico. Son
mejoras marginales sobre un recordatorio, y ninguna justifica por sí sola el
riesgo que se corrió.

**Donde estuvo el valor de verdad:** descifrar un protocolo propietario sin
osciloscopio, con el ESP32 haciendo de instrumento. Eso no es automatizar un
escritorio, es ingeniería inversa de hardware — y queda publicada de forma que a
otra persona con el mismo escritorio le ahorra semanas.

**Y una cosa más, incómoda pero útil:** este repositorio documenta sus errores.
Un ADR que declaró "no hay daño" tras evaluar una sola vía de daño. Dos
diagnósticos elaborados que resultaron ser un puente de estaño y un chip flojo
en la protoboard. Un limitador de seguridad prometido durante semanas que nunca
se implementó. Todo con su fecha y su porqué, sin corrección silenciosa.

**Si se repitiera:** el buffer de protección de la sonda ([ADR-031](DECISIONS.md))
iría montado **antes** de soldar nada al mando, no anotado como mejora futura.
Esa es la lección que costó los tres días.

---

## 2026-08-23 (último) — El ESP32 cede el paso al mando, y la regla nueva

**Regla fijada por el propietario, ya en el CLAUDE.md: toda implementación pasa
revisión adversarial antes de darse por terminada.** Esta entrada es la primera
bajo esa regla, y la revisión volvió a pagar.

### La cesión al mando manual

El sniffer ve todas las teclas del bus, propias y humanas. Ahora, si durante un
viaje nuestro aparece **una tecla que el ESP32 no ordenó**, cede el paso: suelta
su objetivo, no frena (la pulsación humana ya frenó), no persigue. Antes tardaba
9 s en concluir "no avanza" y metía un toque que nadie pidió.

Sensor nuevo: **`uso_manual`** — segundos desde la última vez que una persona
tocó el mando, para que las automatizaciones no se peleen con ella.

**Verificado en vivo**: viaje continuo bajando, pulsación humana, y el resultado
publicado fue `ultimo_freno: mando manual`, `quieto`, `uso_manual: 10`.

**Verificado de regalo**: en un intento anterior el viaje corrió completo sin
observador y **el límite de software frenó solo en el tope** (`limite superior`).

### La revisión de esta implementación encontró seis cosas — dos graves

1. ⚠️ **El "rechazo" del OTA en movimiento era el peligro que decía evitar**:
   reiniciar abre los canales y abrir no frena. Ahora **frena primero** (un toque
   real) y deja pasar la actualización sobre un escritorio parándose
2. ⚠️ **Una tecla manual decodificada justo antes de lanzar un viaje** lo habría
   desarmado sin frenarlo (la pulsación era anterior al pulso). Se absorbe al
   lanzar
3. `parar` recibido durante el frenado podía dejar que un reobjetivo pendiente
   arrancara viaje después del stop — el stop va primero ahora
4. La ventana de "frenado confirmado" (1.5 s) era menor que el periodo de
   actualización del display en movimiento (~1.47 s/cm): podía confirmar un
   freno sobre un escritorio a plena marcha. Subida a 3 s
5. Una atribución de tecla nuestra que no llegara a capturarse dejaba 1.5 s en
   los que una pulsación humana se atribuía a nuestro canal. Ventana acortada
6. `uso_manual` con `millis()` habría dicho "usado hace segundos" cada 49.7
   días. Pasado a reloj de 64 bits

**Aceptado sin arreglar** (anotado): el mismo desbordamiento en `altura_edad` es
cosmético — sus consumos reales son ventanas de segundos, inmunes al envolvido.

### Panel

Añadidas a la sección **Estudio** del panel Casa dos tarjetas: el campo **Ir a
altura** (muestra y fija, en cm) y el **indicador de movimiento**. Nada más, a
propósito: el resto vive en la página del dispositivo.

---

## 2026-08-23 (cierre) — Publicado en GitHub y actualizaciones por red

**Repositorio público:** https://github.com/cesareyeserrano/esp32-desk-controller

Auditoría antes de publicar: sin credenciales en nada versionado (`secrets.h`
ignorado, con plantilla `.example`), sin material de terceros —tres fotos de un
anuncio de tienda quedaron fuera; el datasheet se conserva porque el original ya
devuelve 403—, `.gitignore` completo y licencia MIT.

**README reescrito**: el que había era de julio y describía la fase 2 con
valores de resistencia ya corregidos por [ADR-022](DECISIONS.md). El viejo quedó
en [historia/](historia/).

### OTA: se acabó el cable

Se implementó actualización por red ([ADR-034](DECISIONS.md)), y la razón de
fondo importa: **el procedimiento de flasheo era más peligroso que casi
cualquier fallo del firmware.** Cada corrección obligaba a mover el USB entre el
cargador y el Mac, y cada tránsito reproduce la condición del
[ADR-019](DECISIONS.md) —ESP32 sin alimentar con la sonda sobre un bus vivo—,
que ya costó dos días de diagnósticos falsos.

⚠️ **La actualización se rechaza si el escritorio está en movimiento.** Aplicarla
reinicia el chip, el reinicio abre los canales, y **abrir un contacto no detiene
el movimiento continuo**: una actualización a mitad de viaje dejaría el
escritorio corriendo hasta el tope sin supervisión.

**Verificado el mismo día**: actualización completa por red a `192.168.1.23`,
autenticada, con el ESP32 reconectando solo al broker. También se publica ahora
la IP como entidad, sin la cual no se sabe a dónde actualizar.

Coste: la flash pasa del 69% al 74%. El cable solo hace falta ya para el primer
flasheo de una placa nueva y para leer el puerto serie.

---

## 2026-08-23 (madrugada) — Segunda ronda adversarial, sobre todo el sistema

Antes de publicar en GitHub se pidió una última revisión adversarial, esta vez
del sistema completo: firmware ya corregido, herramientas Python y preparación
de publicación. Ocho ángulos nuevos. **Detalle completo en la ronda 2 de
[REVISION_FIRMWARE_2026-08-23.md](REVISION_FIRMWARE_2026-08-23.md).**

**El hallazgo que justifica el hábito: los arreglos de la ronda 1 introdujeron
regresiones**, y tres ángulos las encontraron por separado. El estado BRAKING
—correcto en su idea— rompió el reobjetivo (`ir:N` en viaje se descartaba en
silencio, la vuelta del mismo C3 que decía arreglar), dejó el ajuste fino a
merced de una carrera de temporizadores que en las pruebas en vivo ganó por
casualidad, y HA veía "quieto" mientras se frenaba. **Un `parar` podía además
ejecutar después un `subir` que hubiera quedado encolado.** Todo arreglado y
compilado.

**Las herramientas habían quedado desfasadas del firmware**: `pulse_loop`
enviaba más rápido de lo que duraba el pulso (cierres pegados ≈ tecla
mantenida), `verificar_canales` exigía una precondición que ya no existía y
dejaba viajar las memorias 5 s antes de frenar, y los dos guiones de viaje
competían con la máquina de estados del firmware — retirados a `tools/legacy/`.

**Unificación de seguridad**: todo toque inofensivo va al canal medido (bajar,
300 ms) y los dígitos por serie son siempre de 300 ms; mover, solo por las vías
supervisadas.

**Auditoría de publicación limpia**: sin secretos en el repo, `secrets.h`
ignorado con plantilla, `.gitignore` completo.

✅ **Flasheado y verificado en vivo el mismo día.** El reobjetivo, que era la
regresión principal, quedó comprobado de punta a punta:

```
[VIAJE subiendo desde 80 cm, objetivo 95]
[MQTT cmd: ir:88]                       <- llega a mitad de camino
[FRENO: reobjetivo] altura 90 cm
[VIAJE corto a 88 cm: por toques]       <- el objetivo SOBREVIVE al freno
[ajuste: 88 cm alcanzados]
```

Antes habría frenado en 90 y descartado el 88 en silencio mientras HA lo mostraba
como aceptado. De paso se ve encadenado el arreglo A3: 90→88 son 2 cm, así que
fue por toques en vez de otro pulso continuo.

También verificado: un comando durante un viaje frena (`[FRENO: parado a mano]`)
usando el canal de 300 ms medido, y el viaje normal sigue clavando el objetivo
(92→80 exactos).

Corregido además un texto de ayuda que seguía anunciando los dígitos como pulsos
de 800 ms cuando ya son toques de 300.

---

## 2026-08-23 (noche) — Revisión adversarial, y nueve arreglos verificados

A petición de quien tiene el escritorio, **antes de seguir con HA se hizo una
revisión adversarial del firmware** — cinco pasadas independientes, consolidadas
en [REVISION_FIRMWARE_2026-08-23.md](REVISION_FIRMWARE_2026-08-23.md).

**El veredicto le dio la razón: no estaba listo.** Lo más grave: el limitador de
pulso que [SEGURIDAD.md](SEGURIDAD.md) atribuía al watchdog de
[ADR-024](DECISIONS.md) **no existía** — se decidió en agosto y nadie lo
implementó. Y tres botones de HA (`refrescar`, `continuo_subir`,
`continuo_bajar`) **nunca funcionaron**: una guardia de longitud los descartaba
sin log.

**Nueve hallazgos arreglados y verificados en vivo la misma noche** (tabla de
estado en el informe). Los tres que más importan:

- **El watchdog existe por fin**: cada pulso arma el task WDT con `ancho+1 s`;
  un cuelgue con el pin alto termina en reinicio y canal abierto
- **El ancho del pulso ya no depende de la impresión serie**: se captura durante
  el contacto y se decodifica después de soltar, y el ancho conseguido se mide
  — un desborde >100 ms se reporta con `[!!]`
- **El filtro de transitorios del display**, verificado en vivo: "199" y "109"
  aparecieron en la transición 99→100 durante un viaje real y no lo perturbaron

**Primer viaje desde HA con `continuo_subir` en la historia del proyecto**
(nunca había llegado al firmware), frenado con `parar` — que ahora frena con
300 ms en vez de mover el escritorio hacia arriba.

**Actualización de la misma noche: los siete restantes también quedaron
cerrados y verificados — 16 de 16.** Entre ellos, tres con verificación en vivo:
los viajes cortos van por toques (80→82→80 exactos, sin pulso largo), el freno se
confirma con el estado `BRAKING` antes de armar el ajuste, y `parar` viaja en un
flag propio que la cola no puede pisar. `C`/`D` por serie se **eliminaron**:
2.8 s sostenidos en una memoria quedan a 200 ms de grabarla.

Antes de tocar nada se guardó copia del firmware funcional en
`firmware/backups/desk_sniffer_2026-08-23_9arreglos.ino.bak`, y cada arreglo se
compiló por separado antes del siguiente. **La fase 4 queda desbloqueada.**

**El escritorio queda a 80 cm.**

---

## 2026-08-23 (final) — Todo recuperado: los cuatro canales OK

**Reconectado por pasos, con el mando comprobado entre cada uno**, y verificado:

```
canal 1 (subir)     : 0x47  OK
canal 2 (bajar)     : 0x57  OK
canal 3 (memoria 1) : 0x67  OK
canal 4 (memoria 2) : 0x6F  OK
```

**El canal 3 era el optoacoplador flojo en la protoboard.** Lo sospechó quien
tiene el escritorio; se asentó bien el chip y respondió a la primera.

**Cómo se acotó, y el orden importó:**

1. El pulsador M1 **funcionaba a mano** → el pulsador está bien
2. **Juntando los dos cables** del canal 3 el escritorio obedecía → los cables
   llegan bien al pulsador
3. El firmware disparaba (`[channel 3 -> GPIO25 pulsed]`) → el ESP32 está bien
4. **Quedaba solo el PC817**, y estaba mal asentado

**Antes de cada prueba se mandó `h` y se comprobó que el puerto recibía**
([ADR-026](DECISIONS.md)). En una de las pasadas el puerto no respondía, y eso
habría dado un canal 3 "muerto" que no lo estaba: **la regla evitó un falso
diagnóstico más**.

### Estado al cerrar

- **Mando funcionando**, con los tres hilos del bus y los ocho de los
  optoacopladores conectados
- **Bus leyéndose**: 227 flancos en CLK, 51 en DIO, líneas identificadas OK
- **Los cuatro canales verificados** contra el bus
- **Escritorio a 85 cm**
- Firmware con WiFi y MQTT, pulso de barrido a 300 ms

⚠️ **Lo del mando yendo y viniendo sigue sin explicación**, y la protoboard sigue
siendo el punto frágil: hoy dio un cortocircuito, un optoacoplador flojo y varias
conexiones que se movían al manipular. **El montaje en placa perforada sigue
pendiente**, y es lo que ataca la causa de fondo.

---

## 2026-08-23 (cierre) — El mando va y vuelve. Causa DESCONOCIDA

⚠️ **No se sabe qué es.** Solo está establecido el hecho: **el mando deja de
funcionar y vuelve a funcionar**, al menos cinco veces en el día.

**Se han propuesto tres explicaciones a lo largo de estos dos días y las tres
resultaron falsas o indemostradas:** chip quemado, latch-up, y puente de estaño.
Una cuarta —contacto marginal o pista agrietada— **encaja con lo observado pero
tampoco está comprobada**, y se anota como lo que es: una conjetura más.

**Lo único medido y firme:**

- El mando muere y revive, siempre **alrededor de manipulaciones**, nunca durante
  el uso normal
- **Una vez revivió sin que se tocara nada eléctrico**
- Cuando funciona, funciona **completo**: display, botones, y grabar y recuperar
  memorias
- Los cortes largos de corriente a veces lo devuelven y a veces no
- Una limpieza con más de 30 min de secado **no lo arregló**

**Lo que sí está descartado, con medición:** los cuatro PC817, el divisor de la
sonda, el firmware, MQTT, Home Assistant y la carga de Ultron. Y la caja de
control está sana.

**Recuento del día: el mando murió y revivió al menos cinco veces**, siempre
alrededor de manipulaciones y nunca durante el uso normal.

| Observado | Puente de estaño | Contacto/pista marginal |
|---|---|---|
| Muere al manipular | encaja | encaja |
| Revive al limpiar | encaja | encaja (se movieron los cables) |
| **Revive solo, sin tocar nada** | **no encaja** | **encaja** |
| Funciona perfecto a ratos, con memorias | regular | encaja |

**Nada quemado. Nada que comprar.** El chip, la caja, los optoacopladores y el
firmware están sanos: todos los diagnósticos de daño de estos dos días fueron
falsos.

### La medición pendiente, y es la que cierra el caso

**Mando desconectado del cable y sin corriente**, multímetro en ohmios, midiendo
la continuidad de las pistas **mientras se mueven los cables**:

- Pin del **rojo** en el conector → **pata 5** del chip (CLK)
- Pin del **verde** en el conector → **pata 6** del chip (DIO)

**Si el valor baila al mover, ahí está.** Reparación: un cable fino puenteando la
pista de extremo a extremo. Sin comprar nada.

⚠️ **No probar canales mientras el contacto sea intermitente.** Cualquier
resultado sobre un montaje que va y viene no vale, y es exactamente el error que
costó dos días de diagnósticos falsos.

---

## 2026-08-23 (cierre, primera versión) — Cortos intermitentes (hipótesis superada)

**El mando volvió a morir** tras desmontar para mejorar los cables de los
pulsadores, y **volvió a revivir después de limpiar la zona de los tres cables
del bus**. Es la **segunda vez** que ocurre exactamente eso.

**Esa repetición es la mejor evidencia del episodio entero.** Lo que encaja con
todo el comportamiento errático de dos días es un **puente intermitente por
residuos de soldadura** —flux, hilillos de estaño— que hace contacto según cómo
queden los cables al moverlos.

**Explica lo que ninguna otra hipótesis explicaba:**

| Observado | Encaja |
|---|---|
| Muere al manipular y reconectar, no con el uso | Sí: el puente se forma al mover los cables |
| Revive al limpiar, sin tocar nada más | Sí, dos veces |
| Revive tras cortes largos, a veces | Sí: el contacto marginal cambia al enfriarse o moverse |
| CLK clavado en 0.7–0.9 V | Sí: puente resistivo, no unión de silicio |
| Diagnósticos previos —latch-up, chip quemado— | **Todos falsos** |

**El corto verde-amarillo del mediodía era esto mismo**, reducido pero no
eliminado.

### Lo que se hace en el montaje nuevo

Ya estaba decidido rehacer el cableado. Con esto, tres requisitos concretos:

- **Limpiar con alcohol isopropílico** las soldaduras del mando. El flux se
  vuelve conductor con la humedad y produce cortos que aparecen y desaparecen
- **Inspeccionar con aumento** —lupa o la cámara del móvil— antes de cerrar. Un
  hilillo de estaño de medio milímetro basta
- **Separar físicamente los tres cables** al salir del mando, y **que el amarillo
  no se acerque a ninguno**: es el que hace daño cuando toca donde no debe

### Y la regla que sigue costando

Durante estas pruebas el mando estuvo **con los hilos del bus conectados y el
ESP32 sin alimentar** — el escenario del [ADR-019](DECISIONS.md), que por sí
solo ya tumba el mando sin dañarlo. **Se dio varias veces sin querer**, al
desconectar el USB para trabajar dejando los hilos puestos.

**Los hilos del bus salen ANTES de quitar el USB, y entran DESPUÉS de ponerlo.**
No es una precaución de diagnóstico: es lo que evita perseguir fantasmas dos días
seguidos.

---

## 2026-08-23 (noche) — Todo recuperado menos el canal 3

**Reconexión por pasos**, comprobando el mando entre cada uno, después de limpiar
el cortocircuito verde–amarillo:

| Paso | Resultado |
|---|---|
| Mando solo, uso exigente | **OK** — alturas y memorias |
| + USB del ESP32 | **OK** |
| + hilo azul (masa) | **OK** |
| + hilo verde (DIO) | **OK** — probado con memorias, era el sospechoso |
| + hilo rojo (CLK) | **OK** |
| + los ocho de los optoacopladores | **OK** |

**El bus volvió**: 228 flancos en CLK, 51 en DIO, 360 transacciones y la
autocomprobación de líneas en OK. Después de un día entero con el bus mudo.

### Los canales: tres de cuatro

| Canal | Pin | Resultado |
|---|---|---|
| 1 | GPIO 27 | **0x47 OK** |
| 2 | GPIO 26 | **0x57 OK** |
| **3** | **GPIO 25** | ⚠️ **SIN RESPUESTA** |
| 4 | GPIO 33 | **0x6F OK** |

**El canal 3 dispara pero la caja no ve tecla**, y el display **ni se despierta**
—con los otros canales sí—. **El pulsador M1 funciona a mano**, así que el
pulsador está bien: el circuito no llega a cerrarse entre el PC817 y él.

Se cambió el cable y siguió igual. **Falta la medición que lo parte en dos**:
ohmios entre las patas 3 y 4 del PC817 del canal 3, mientras `tools/pulse_loop.py 3`
lo dispara en bucle.

- **~150 Ω** → el optoacoplador conduce; el fallo está en los cables al pulsador
- **Siempre OL** → el optoacoplador no conduce; mirar su LED (patas 1 y 2)

### Se para para rehacer el cableado

Se decide **rehacer el sistema de cableado** en vez de seguir parcheando. Es
sensato: **la protoboard ha sido el eslabón frágil de todo el día** — el corto
verde-amarillo, el canal 3 suelto, y las conexiones que se movían al manipular.

### Un aviso que se cumplió

El puerto serie volvió a no recibir comandos en mitad de las pruebas
([ADR-026](DECISIONS.md), sin causa identificada). Se detectó porque **faltaba la
línea `[channel 3 -> GPIO25 pulsed]`** en la salida, y al mandar `h` primero se
confirmó. **La regla de comprobar el puerto antes de fiarse de un canal evitó
otro diagnóstico falso**, esta vez sí.

---

## 2026-08-23 (tarde) — FALSA ALARMA: el mando estaba sano. Era un cortocircuito

⚠️ **La entrada de abajo —"El mando está dañado"— es FALSA, y el diagnóstico de
latch-up que contiene también.** Se deja escrita porque el recorrido equivocado
es lo que más enseña de todo esto.

**Lo que era:** un **cortocircuito entre el hilo verde (DIO) y el amarillo
(5 V)**, casi con seguridad un puente de estaño de las soldaduras hechas al
cerrar la tapa. Cinco voltios permanentes metidos en la línea de datos.

**Lo encontró quien tiene el escritorio, mirando el hardware**, mientras yo
razonaba sobre curvas de diodo y estructuras parásitas a partir de dos números
sueltos.

**Verificado tras deshacer el corto:** el mando funciona con normalidad, y **se
grabaron y recuperaron dos memorias**. Grabar exige mantener pulsado 3 s con el
chip leyendo el teclado de forma sostenida: es la prueba más dura que hay, y la
pasó.

### Por qué el diagnóstico de latch-up era plausible y aun así falso

Todo lo observado encajaba: revivía con cortes largos, moría al primer uso, CLK
clavado en 0.7–0.9 V. **El corto explica lo mismo, mejor y sin daño**: 5 V
permanentes en DIO dejan la comunicación imposible, y desconectar todo un rato
cambiaba lo suficiente el estado como para que pareciera revivir.

**El error de método:** se construyó una explicación elaborada sobre dos
mediciones de tensión, en vez de buscar primero la explicación mecánica más
simple —un puente entre dos hilos contiguos— en un montaje **que se acababa de
manipular a mano**. La navaja de Occam apuntaba al estaño, no al silicio.

**La segunda vez que pasa lo mismo en esta sesión.** El 2026-08-21 un canal
"muerto" resultó ser un fallo del propio sniffer. La regla que salió entonces
—*cuando el mundo físico y la captura discrepen, el sospechoso es la captura*—
se queda corta. **Ampliada: cuando algo falla justo después de tocar el
hardware, el primer sospechoso es lo que se tocó.**

### Qué queda en pie de todo aquel diagnóstico

- **[ADR-031](DECISIONS.md) sigue siendo válido**: el "no hay daño" del ADR-019
  evaluaba solo el efecto térmico y no el latch-up. Que esta vez no fuera eso no
  devuelve la validez a un razonamiento incompleto. **La regla de conexión
  —USB primero, hilos después— se mantiene.**
- **[ADR-032](DECISIONS.md) queda EN SUSPENSO.** Se escribió para permitir que
  el ESP32 sustituyera a un mando muerto. **El mando no está muerto**, así que
  su condición de activación no se cumple y **el [ADR-011](DECISIONS.md) vuelve
  a aplicar entero: no se escribe en el bus.**
- **El pull-up de la caja, 21 kΩ en CLK y 22 kΩ en DIO, sigue medido y es un
  dato bueno** — el único hallazgo útil de todo el episodio.
- **No hace falta comprar nada.** Ni transistores, ni chip, ni mando.

---

## 2026-08-23 — El mando está dañado (FALSO — ver la entrada de arriba)

**Confirmado por la mañana, con el mando completamente solo** —sin ESP32, sin
hilos del bus, sin nada conectado salvo sus cables soldados por dentro—:

- Tras un corte de corriente largo, **revive**
- Responde **a una sola pulsación**, hace lo que debe
- **Y muere.** No vuelve hasta otro corte largo

**Es un daño, no un enclavamiento transitorio.** Se cierra el incidente del
2026-08-22 con esa conclusión.

### Diagnóstico: latch-up en el chip del mando

**El bus está mudo porque la caja de control no puede transmitir.** El reloj lo
genera ella —es el maestro ([PROTOCOLO.md](PROTOCOLO.md))— y para generarlo
necesita poder subir CLK a 5 V. Medido en el mando enfermo: **CLK clavado en
0.7–0.9 V**, con la forma de una unión conduciendo. Con la línea sujeta abajo, la
caja no puede hablar: **cero flancos**, que es exactamente lo que ve el sniffer.

**El comportamiento es el de un latch-up**, una estructura parásita que se activa
dentro del chip, conduce a masa y **solo se apaga cortando la alimentación por
completo**:

| Observado | Encaja con latch-up |
|---|---|
| Revive solo tras cortes de corriente **largos** | Sí: hay que descargar del todo |
| Muere **a la primera pulsación** | Sí: se redispara en cuanto el chip trabaja |
| CLK clavado en 0.7–0.9 V | Sí: es la caída de la estructura conduciendo |
| DIO seguía a 4.0 V | Sí: afecta a un pin, no a todo el chip |

**Es daño permanente.** No hay reset, firmware ni configuración que lo arregle.

### Causa probable, y no es cómoda

**Un latch-up se dispara inyectando corriente por un pin.** El proyecto tiene
documentado en [ADR-019](DECISIONS.md) que **con el ESP32 sin alimentar y el bus
encendido, el diodo de protección de su GPIO inyecta corriente en la línea**. Se
escribió como *"no hay daño — la corriente son ~210 µA"*.

**Esa condición se dio varias veces el 2026-08-22**, en las conexiones y
desconexiones para cerrar la tapa del mando.

**No está demostrado que fuera eso.** Pero es el mecanismo más plausible, viene
de nuestro montaje, y **contradice el "no hay daño" del ADR-019** — que valoraba
la corriente pero no el riesgo de latch-up. **Ese ADR necesita corrección**, y se
deja anotado aquí en vez de dejarlo pasar.

### Qué está descartado y qué no

**Descartado que lo mantenga roto nada nuestro:** falla con el ESP32
desconectado. El A/B con el firmware viejo ya no hace falta.

**No descartado que algo de lo nuestro lo dañara.** Coincidió en el tiempo con
tres cosas: cerrarle la tapa con once cables soldados dentro, las pruebas de
movimiento continuo y viajes del 2026-08-22, y en algún momento una pulsación del
botón de reset. **La causa no se ha determinado**, y se deja así escrito en vez
de elegir la hipótesis más cómoda.

**La caja de control está sana.** Movió los motores la noche del 22. Es la pieza
sin repuesto fácil, y no es la que ha fallado.

### Lo que sigue en pie

Todo lo verificado hasta ayer sigue siendo válido: el protocolo, la sonda, los
cuatro canales, la integración con Home Assistant, el control por altura. Nada
de eso dependía de *este* mando en concreto.

### Siguiente paso

**Conseguir un mando de repuesto.** Queda abierto investigar compatibilidad:
`JK-CH506` no aparece en el catálogo público de Jiecang, así que hay que buscar
por la caja de control o por el chip (AiP650E / TM1650) antes de comprar.

Alternativa a tener en cuenta: con el protocolo descifrado y la caja como
maestro, **un mando propio** —TM1650 más una matriz de pulsadores— es
técnicamente viable. No se decide aquí; se anota.

---

## 2026-08-22 (madrugada) — INCIDENTE ABIERTO: el mando se enclava

⚠️ **Sin resolver al cerrar la sesión.** El mando funciona a ratos y se queda
mudo. **Se recupera cortando la corriente**, no soltando cables.

### El síntoma, y su patrón

El mando dejó de responder después de cerrarle la tapa. A lo largo de la noche
el cuadro se repitió varias veces:

1. Ciclo de corriente (desenchufar ~30 s) → **el mando revive**
2. Se usa un rato —subir, bajar, M1— → **deja de responder**
3. Vuelta al punto 1

**Que se recupere con un corte de corriente y no soltando cables apunta a un
enclavamiento del chip, no a daño.** Es repetible, y un fallo repetible se caza.

En un momento intermedio se vio el estado más informativo de todos: **display
encendido y botones muertos**. El chip dibuja el display, así que estaba vivo; lo
que fallaba era el escaneo del teclado.

### Lo descartado, con medición

| Componente | Estado | Prueba |
|---|---|---|
| **Caja de control** | **Sana** | Movió los motores. Es la pieza sin repuesto |
| **Los cuatro PC817** | **Sanos** | `OL` entre patas 3 y 4 en reposo, **con y sin ESP32 alimentado** |
| **Divisor de la sonda** | **Correcto** | 16.5 kΩ al GPIO, 43.5 kΩ del bus a masa |
| **Ultron / MQTT** | **No relacionado** | El `python3` al 99% llevaba **35 horas** corriendo, de `hermes`. Mosquitto: 0.03% |
| **Chip del mando** | **Vivo** | Revive entero tras cortar corriente |

Niveles medidos con el bus caído: **verde (DIO) 4.0 V**, **rojo (CLK) 0.7–0.9 V**.
El verde a 4 V lo sostiene el pull-up **interno del chip**, así que había
alimentación. El rojo clavado en torno a 0.8 V tiene la forma de una unión
conduciendo.

### Lo que queda sin descartar

**Los cables soldados dentro del mando**: los ocho de los pulsadores y las tres
derivaciones del bus. Es lo único de nuestro montaje que sigue tocando la placa
del mando, y no se ha aislado.

**Siguiente paso concreto:** desconectar los ocho cables **del lado de los
PC817** —sin desoldar nada del mando—, dejar el ESP32 fuera del todo, hacer
ciclo de corriente y **usar el mando a mano varios minutos**.

- **Aguanta** → el problema está en nuestro montaje
- **Se vuelve a enclavar** → está en las soldaduras internas

### Cuatro errores de diagnóstico, para no repetirlos

**1. No apliqué [ADR-019](DECISIONS.md), que describe este síntoma literalmente.**
Dice que con el ESP32 sin corriente y la sonda sobre un bus encendido, el mando
falla, y que *"el síntoma aparecería justo después de soldar y apuntaría a las
soldaduras"*. Es exactamente lo que pasó y lo que estuvimos persiguiendo. **El
ADR existía para evitar esta confusión y no lo consulté.**

**2. Di el divisor por 9.1 kΩ durante todo el diagnóstico.** Lo montado son
**16.3 kΩ** desde [ADR-022](DECISIONS.md), del 2026-08-06. [HARDWARE.md](HARDWARE.md)
conservaba el diagrama viejo y el error se propagó a las cabeceras de las
capturas escritas ese mismo día. Corregido en HARDWARE.md; **las capturas de hoy
que dicen "sonda 9.1k/27k" están mal en ese dato**.

**3. Interpreté mal dos medidas correctas.** Dije que 43.5 kΩ del bus a masa era
anómalo cuando es exactamente 16.3 + 27 en serie, y estuve a punto de mandar a
buscar un corto que no existía.

**4. Exoneré los optoacopladores demasiado pronto**, razonando que el mando
revivía con ellos conectados. **Revivir no es aguantar**, y el patrón es
precisamente que aguanta un rato. Quedaron descartados después, ya con medición.

### Cierre de la noche: el mando ya no revive

Al final de la sesión **el ciclo de corriente dejó de resucitarlo**. Los
enclavamientos anteriores se limpiaban desenchufando 30 s; el último no. Estado
al parar: **mando sin responder, ni display ni botones**.

Queda **preparado el experimento A/B** que decide si el firmware de hoy tuvo que
ver: el ESP32 tiene cargado `ino.before_mqtt` — el firmware de **antes** de
WiFi, MQTT y viajes, el que corrió toda la verificación de los cuatro canales
sin un solo fallo del mando. Sin radio: ni HA ni el broker pueden tocarlo.

**Al retomar, en este orden:**

1. Ciclo de corriente largo al escritorio (minutos, no segundos) y probar el
   mando **solo**, sin conectar nada
2. Si revive → reconectar (USB → hilos → corriente) y **usarlo a mano varios
   minutos** con el firmware viejo:
   - aguanta → el sospechoso es el código nuevo; se reintroduce por partes
   - se enclava → el código de hoy queda absuelto; el foco es el hardware
3. Si no revive ni solo → medir en el mando: continuidad de sus pistas junto a
   las soldaduras, y los 5 V del hilo amarillo contra el azul en su conector
4. Si se confirma muerto: **un mando de repuesto Jiecang es comprable**. La
   caja de control —la pieza sin repuesto— está sana: movió los motores esta
   misma noche.

Las dos hipótesis siguen abiertas y ninguna está probada: **(a)** las
soldaduras/cables dentro del mando tras cerrar la tapa, **(b)** el firmware
nuevo de hoy. La cronología apoya (b); el reloj clavado a 0.9 V con todo
desconectado apoya (a). **El A/B de arriba es lo que las separa.**

### Estado físico al cerrar

Mando **destapado**, con sus cables soldados. ESP32 desconectado del bus. Los
ocho cables de los pulsadores, conectados o no según dónde se dejara la prueba.
Escritorio en el tope inferior tras una recalibración: en algún momento se pulsó
el botón de **reset** —el que [ADR-008](DECISIONS.md) manda no cablear— y el
escritorio bajó del todo, aunque **no está confirmado** que fuera esa pulsación
la que lo provocó.

**Pendiente por eso:** comprobar si el reset alteró M1 y M2. Con
[ADR-029](DECISIONS.md) el sistema ya no depende de esas alturas, pero conviene
saberlo.

---

## 2026-08-22 (noche) — El escritorio llega a Home Assistant

**Objetivo:** abrir la fase 4 y llevar el estado a HA.

### Se eligió MQTT, y la decisión se recomendó dos veces al revés

Se entró por SSH a **Ultron** para ver la instalación real en vez de suponerla, y
cambió el cuadro:

| | Estado |
|---|---|
| Home Assistant | 2026.5.2, **en Docker, red `host`** |
| Add-ons | **Imposibles**: es HA Container, sin Supervisor |
| ESPHome | Integración **ya funcionando**, con un M5Stack Atom Echo |
| InfluxDB | Integrado |
| MQTT | **No existía** |

Primero se recomendó MQTT sin haber mirado. Luego ESPHome, al ver que su
integración ya estaba. Y finalmente **MQTT otra vez**, al pesar el argumento que
decide: **el sniffer bloquea hasta 2.8 s por pulso largo y 250 ms armando cada
ráfaga, y ESPHome espera componentes que devuelvan enseguida**. Meterlo ahí
obliga a reescribir como máquina de estados el código con timing crítico que ya
costó una tarde de depuración.

Razonado en [ADR-030](DECISIONS.md), **incluido el vaivén**: la causa fue
recomendar antes de mirar, y sin eso el ADR parecería más limpio de lo que fue.

### Verificado eslabón por eslabón, no de golpe

1. **Broker** `eclipse-mosquitto:2` en Ultron, con volumen propio, usuario y
   contraseña. Probado con `mosquitto_pub`/`sub`.
2. **HA conectado** al broker.
3. **Discovery → entidad**, con un sensor de prueba **y sin el ESP32 de por
   medio**. Se creó `sensor.escritorio_jiecang_prueba_escritorio`, y luego se
   retiró publicando un mensaje vacío retenido.
4. **Y por último el firmware.**

Hacerlo así tuvo premio inmediato: en el paso 3 se dio por fallido el discovery
porque la entidad no aparecía a los 6 segundos. **Aparecía a los 20.** Se
corrigió sobre la marcha en vez de ir a buscar el fallo al firmware, que ni
existía todavía.

### El resultado

**Siete entidades creadas solas en HA**, y la cadena entera funcionando: altura,
antigüedad de la altura, display despierto, malformadas del bus, transacciones,
RSSI y uptime.

**La altura salió `unknown` mientras el display estuvo dormido**, que es el
comportamiento correcto y no una avería: con el display apagado la altura **no
está en el bus**. Tras el toque de refresco publicó **110 cm** —la altura real, a
la que se había movido a mano— junto con su antigüedad en segundos.

### Y se cerró la duda que quedaba abierta

Esa mañana se había medido que el WiFi no degrada la captura, **pero en modo AP
sin clientes**, y quedó escrito que no se podía dar la arquitectura por buena sin
medirlo en STA con tráfico real. Medido:

| | Malformadas |
|---|---|
| Sin WiFi (referencia) | 0.67% |
| WiFi modo AP sin clientes | 0.93% |
| **WiFi STA + publicando MQTT** | **0.60 – 0.68%** |

**Publicar no degrada nada.** RSSI de -57 a -60 dBm. El firmware ocupa el 69% de
la flash y el 16% de la RAM.

Captura:
[capturas/2026-08-22-mqtt-primera-conexion.log](capturas/2026-08-22-mqtt-primera-conexion.log).

### Los controles: seis botones en Home Assistant

Se añadieron los cuatro que se pidieron —subir, bajar, M1 y M2— **más dos que no
se pidieron y son necesarios**:

- **`parar`**, porque con movimiento continuo cerrar un contacto es lo único que
  detiene el escritorio. Si M1 y M2 son pulsables desde un móvil, la parada tiene
  que estar en la misma pantalla.
- **`refrescar altura`**, el toque de 300 ms que despierta el display sin mover
  nada. Sin él, tras un rato de inactividad HA no puede saber dónde está el
  escritorio.

**Verificado de punta a punta**, mandando los comandos desde Ultron igual que los
mandaría HA:

```
refrescar -> CHANNEL 2  0x57
subir     -> CHANNEL 1  0x47
subir     -> CHANNEL 1  0x47
bajar     -> CHANNEL 2  0x57
```

**Trece entidades** en total. Captura:
[capturas/2026-08-22-controles-mqtt.log](capturas/2026-08-22-controles-mqtt.log).

**Decisión de seguridad, tomada al implementar:** se expone **solo el toque,
nunca el pulso largo**. Así nada pulsable desde un móvil puede arrancar
movimiento continuo ni sobrescribir un preset. Lo que sí puede pasar es que un
M1/M2 arranque un viaje de hasta 44 cm — lo ejecuta la caja de control sola — y
por eso el botón de parar no era opcional.

**Dos comandos nuevos por serie:** `d` republica el discovery a demanda, y cada
publicación informa de si tuvo éxito. Salió de un diagnóstico: los botones no
aparecían y hacía falta distinguir "no se publicó" de "se publicó y HA no lo ha
procesado". **Era lo segundo** — HA tarda unos veinte segundos, y ya se había
dado por fallido antes por mirar demasiado pronto.

### Antes de todo esto: se cerró la tapa del mando

Con los cuatro pulsadores soldados se cerró el mando, y **antes de atornillar** se
comprobaron los cuatro canales: los cuatro OK, 94 cm antes y después. Queda como
herramienta reutilizable en `tools/verificar_canales.py`, que frena los viajes de
las memorias en cuanto delatan su código.

### Decidido: las memorias del mando son opacas

A propuesta de quien tiene el escritorio: **el sistema no asocia M1 y M2 a
ninguna altura** ([ADR-029](DECISIONS.md)). Si nunca afirma que M1 vale 80, no
puede mentir cuando el reset o una regrabación lo cambien. Los presets por
software son independientes y no pueden sobrescribir las memorias del mando.

---

## 2026-08-22 — Los cuatro canales verificados: la fase 3 queda cerrada

**Objetivo de la sesión:** soldar los tres pulsadores que faltaban y comprobar
los cuatro canales. **Conseguido, y a la primera en los tres nuevos.**

Se soldaron subir y las dos memorias, y de paso se ordenaron las soldaduras
dentro del mando, que está casi listo para cerrarse.

### El barrido

Con el procedimiento que dejó escrito la sesión anterior: **`h` primero** para
comprobar que el puerto recibe ([ADR-026](DECISIONS.md)), y **pulso de 300 ms**
para identificar — con ese ancho subir y bajar registran la tecla en el bus
**sin mover el escritorio** ([ADR-027](DECISIONS.md)), así que se puede
identificar sin efectos. Las memorias sí arrancan viaje con cualquier toque, y
eso se avisó antes de dispararlas.

| Canal | Pin | Botón | Código | Comprobación |
|---|---|---|---|---|
| 1 | GPIO 27 | **Subir** | `0x47` | KI1 / DIG4. No movió: 073 |
| 2 | GPIO 26 | **Bajar** | `0x57` | KI3 / DIG4. Verificado el 2026-08-21 |
| 3 | GPIO 25 | **Memoria 80 cm** | `0x67` | KI5 / DIG4. Viajó 073 → 080 |
| 4 | GPIO 33 | **Memoria 117 cm** | `0x6F` | KI6 / DIG4. Viajó 080 → 117 |

**Los cuatro códigos coinciden con lo que [PROTOCOLO.md](PROTOCOLO.md) predecía**
desde el 2026-08-06, decodificado leyendo el bus. La predicción se cumplió sin
un solo ajuste.

Captura: [capturas/2026-08-22-cuatro-canales-verificados.log](capturas/2026-08-22-cuatro-canales-verificados.log).

**El reset no está cableado** ([ADR-008](DECISIONS.md)), como estaba previsto.

### Lo que esto cierra

**La fase 3 está completa.** El ESP32 lee la altura del bus y acciona los cuatro
botones. El lazo entero funciona:

- **Leer**: altura verificada contra la pantalla del mando
- **Actuar**: cuatro canales aislados por optoacoplador
- **Realimentar**: `tools/goto_height.py` posiciona en lazo cerrado

**Y resuelve un cabo suelto que llevaba abierto desde el 2026-08-06:** ya se sabe
qué preset va con cada canal —`0x67` es el de 80 cm y `0x6F` el de 117— aunque
**sigue sin saberse cuál de los dos botones físicos es M1 y cuál M2**. Para el
firmware da igual; para la interfaz de la fase 5, no.

### Prueba de recorrido completo, y las memorias identificadas

Con los cuatro canales ya verificados se hizo **una secuencia de seis tramos sin
tocar el mando**, en lazo cerrado: el ESP32 arranca movimiento continuo, lee la
altura del bus y frena solo.

| Tramo | Resultado |
|---|---|
| 117 → tope inferior | **73 cm**, parado en el tope |
| 73 → 80 | frenó en 79, ajustó a **80** |
| 80 → 75 | frenó en 76, ajustó a **75** |
| 75 → tope superior | **118 cm**, parado en el tope |
| 118 → media altura | frenó en 96, ajustó a **95** |
| 95 → tope inferior | **73 cm** |

**Las tres frenadas por altura acertaron el objetivo exacto**: se frena 1 cm
antes por la inercia y se ajusta con toques. 145 cm de recorrido en 4 minutos,
sin un fallo.

**Velocidad medida: 0.68 cm/s**, igual en los dos sentidos — 44 cm en 65 s
bajando, 43 cm en 64 s subiendo. Dato nuevo, no estaba en ningún sitio.

**Esta vez sí se guardó el volcado crudo** (311 KB), corrigiendo la carencia
anotada el 2026-08-21, cuando las pruebas de movimiento solo dejaron la traza de
alturas. Captura:
[capturas/2026-08-22-recorrido-completo.log](capturas/2026-08-22-recorrido-completo.log).
El guion está en `tools/recorrido_prueba.py`.

**Y quedaron identificadas las memorias:** **M1 → canal 3 → 80 cm**, **M2 →
canal 4 → 117 cm**. Se predijo antes de probar y se cumplió.

⚠️ **Dos fuentes distintas, y no conviene mezclarlas.** La relación *canal →
altura* está **medida en el bus**. La relación *botón → canal* la aporta quien
soldó los cables: **la etiqueta del mando no se ve desde el bus**, así que esa
mitad no es una medición sino un dato de quien montó el hardware.

### Apertura de la fase 4: primero medir la radio

Antes de diseñar nada sobre WiFi se midió **el único riesgo capaz de tumbar la
arquitectura entera**: la radio nunca se había encendido en esta placa, el
sniffer muestrea a 4 MHz con las interrupciones apagadas 2 ms por ráfaga, y
**este mismo ESP32 se colgó dos veces el 2026-08-03** por saturación de
interrupciones.

Dos corridas de 60 s, idénticas salvo la radio, con el bus en reposo:

| | Sin WiFi | Con WiFi (AP) |
|---|---|---|
| Ráfagas | 299 | 298 |
| Flancos | 81538 | 81249 |
| Transacciones | 1502 | 1499 |
| **Malformadas** | **0.67%** | **0.93%** |
| Muestras tardías | 15 | 17 |
| Reloj | 137 kHz | 137 kHz |

**Verificado: la radio no degrada la captura.** La diferencia cae dentro del
ruido de fondo documentado el 2026-08-06 (~0.8%). Sin cuelgues ni reinicios.
Coste: programa del 21% al 67% de la flash, RAM del 9% al 16%.

⚠️ **Supuesto, no verificado:** es modo AP **sin clientes**, el caso más suave.
**Falta medirlo en STA con tráfico real**, y hasta entonces la arquitectura no
está confirmada. Captura:
[capturas/2026-08-22-wifi-impacto.log](capturas/2026-08-22-wifi-impacto.log).
Sketch: [../firmware/test_wifi_impact/](../firmware/test_wifi_impact/).

**Y de paso quedó la referencia de qué es un bus sano:** 1502 transacciones por
minuto con 0.67% malformadas. Sin ese número, dentro de tres meses nadie sabrá
si un 2% es normal o es la sonda degradándose.

### Diseño de la integración

Escrito [INTEGRACION_HA.md](INTEGRACION_HA.md) con el catálogo completo: estado,
uso, eventos, controles y diagnóstico, marcando **qué se puede medir ya y qué hay
que implementar**.

**El principio que lo ordena: el ESP32 publica hechos, HA deriva estadísticas.**
HA ya tiene base de datos e historización; lo que acumule el ESP32 se pierde en
cada reinicio, y el firmware con timing crítico conviene que sea pequeño. La
excepción es la distancia total recorrida, que cambia demasiado rápido para
publicarla en crudo.

**Lo que más juego da, y no era evidente:** el sniffer ve las teclas del mando
físico, no solo las que manda el ESP32. HA puede enterarse de que **una persona
ha tocado el mando** y no pelearse con ella.

### Cabo suelto nuevo

Aparecieron **cinco bytes distintos en reposo**: `0x07`, `0x17`, `0x27`, `0x2E` y
`0x2F`. **Ninguno lleva el bit `0x40`, así que ninguno es una tecla pulsada** —el
decodificador los trata a todos como reposo, y por eso no estorban—. Pero no se
sabe en qué se diferencian. Probablemente indican qué columna se está
escaneando. Anotado en [PLAN.md](PLAN.md).

---

## 2026-08-21 (tarde) — Primer canal cerrando el lazo, y tres supuestos caídos

**Objetivo de la sesión:** comprobar el canal 2 recién soldado disparándolo y
leyendo qué tecla ve la caja de control. **Conseguido: responde `0x57`, bajar.**

⚠️ **Esta entrada se escribió a media investigación, con el título "el canal 2
no acciona" y esa conclusión por todo el texto. Era falsa.** Se corrige al
final, en *Desenlace*, y **no se borra lo anterior**: el recorrido equivocado es
la parte útil de la entrada.

**Nota de método, antes que nada.** Durante un rato hubo **dos sesiones
trabajando a la vez** sobre el mismo repositorio y el mismo ESP32. Se detectó a
tiempo y se paró una, pero llegó a subirse firmware desde las dos. **Es
exactamente el escenario que fabrica falsos rastros**: un binario que no
corresponde al `.ino` del disco, y mediciones que no se pueden atribuir. En
adelante, una sola sesión toca el hardware.

### Lo que se comprobó, en orden

**1. El comando no llegaba al sketch.** Ni a 460800 ni a 115200, con dos
herramientas independientes (`serial_talk.py` y `arduino-cli monitor`), y con el
`switch` instrumentado para delatar cualquier byte: **cero bytes**. Esto
contradice al [ADR-025](DECISIONS.md), escrito esa misma tarde, que daba 460800
por verificado con 10 comandos de 10.

Se bisecó el firmware entero. Descartados por medición: la velocidad,
`setTxBufferSize`, el `noInterrupts()` de la ráfaga, el bucle que no cede CPU al
planificador, y el adaptador USB-serie —el bootloader recibe a 898.8 kbit/s—.
Después de todo eso **el puerto empezó a recibir de forma fiable, con un cambio
puramente cosmético**, y siguió haciéndolo en 5 pruebas de 5.

**La causa no se identificó.** Está escrito así, sin adornos, en
[ADR-026](DECISIONS.md), junto con la tabla completa de lo que se midió y la
regla que evita repetir el error: **mandar `h` y comprobar que responde, antes
de cualquier prueba de canal.**

**2. El canal 2 no acciona nada.** Verificado, y no es intermitente:

| Qué | Resultado |
|---|---|
| El byte llega al ESP32 | **Sí** — `[RX 0x32 '2']` |
| El canal se dispara | **Sí** — `[channel 2 -> GPIO26 pulsed 300 ms]` |
| La caja de control ve una tecla | **No** — 56 lecturas, todas `4F 17` (reposo) |
| Repetido 3 veces más | Idéntico |

Captura: [capturas/2026-08-21-canal2-no-responde.log](capturas/2026-08-21-canal2-no-responde.log).

**3. Prueba de control: el pulsador a mano sí funciona.** Es lo que separa un
fallo de lectura de uno de accionamiento, y salió limpio:

- **19 lecturas del byte `0x57`** — BAJAR, justo lo que predecía
  [PROTOCOLO.md](PROTOCOLO.md)
- El display se encendió y el escritorio **bajó de 78 a 77 cm**

**Verificado, y corrige una duda razonable:** el sniffer detecta pulsaciones
**aunque el display estuviera apagado** por inactividad. La lectura no es el
problema.

**Conclusión: el fallo está entre el GPIO26 y el pulsador.** No en el bus, no en
el decodificador, no en el puerto serie.

### Correcciones registradas

- **[ADR-025](DECISIONS.md) no se reproduce.** No se toca —los ADR no se
  editan—; lo corrige [ADR-026](DECISIONS.md).
- La captura
  [capturas/2026-08-21-canal2-disparo-bajar.log](capturas/2026-08-21-canal2-disparo-bajar.log)
  queda **marcada INSERVIBLE**, sin tocar su contenido: parecía un canal muerto
  y era el puerto serie. Es el primer caso del error que la regla nueva evita.

### Estado del firmware

`desk_sniffer` a **115200**, con una línea que imprime cada byte recibido
([ADR-026](DECISIONS.md), punto 3). Compila limpio: 21% de programa, 9% de RAM.

### Desenlace: el canal 2 funciona. Era el sniffer.

⚠️ **Todo lo escrito arriba bajo "el canal 2 no acciona" es falso, y se deja
escrito por lo que enseña.** El canal 2 accionaba el pulsador desde el primer
disparo. **El firmware no podía verlo.**

```c
digitalWrite(CH_PIN[idx], HIGH);
while ((uint32_t)(millis() - t0) < PULSE_MS) {
}                                    // <-- vacio
digitalWrite(CH_PIN[idx], LOW);
```

**El bucle del pulso estaba vacío: el sniffer quedaba ciego exactamente durante
los 300 ms en que la tecla estaba pulsada**, y cuando volvía a mirar el bus la
tecla ya estaba suelta. El mecanismo de atribución —la razón entera de meter el
accionamiento en el mismo sketch— **no podía funcionar nunca**.

Por eso la pulsación **a mano sí se veía**: el dedo aguanta segundos y el
sniffer la pillaba entre ráfagas. La del ESP32 dura 300 ms y caía justo en el
punto ciego.

**Lo que lo destapó** fue subir el pulso a 800 ms como prueba: el escritorio bajó
de **085 a 084** y la captura seguía diciendo que no había pasado nada. Un
instrumento que contradice al mundo físico está mal el instrumento.

**Corregido:** el bucle captura y decodifica mientras el contacto está cerrado, y
solo arranca una ráfaga si cabe entera dentro del pulso, para que decodificar no
pueda alargar el contacto. Ese límite es lo que sostiene
[ADR-023](DECISIONS.md). El pulso vuelve a **300 ms**.

**Verificado a la primera con la corrección:**

```
[2.597976] >>> KEY PRESSED KI3 / DIG4   <== CHANNEL 2 answered with 0x57
```

Captura: [capturas/2026-08-21-canal2-verificado-0x57.log](capturas/2026-08-21-canal2-verificado-0x57.log).
**`0x57` es BAJAR. El paso 3 de la fase 3 queda cerrado.**

### Dos correcciones más, del mismo rato

**Estuve a punto de meter un fallo peor al arreglarlo.** Amplié la condición de
atribución de `dat & 0x40` a "cualquier byte distinto de `KEY_NONE`", creyendo
que `0x27` era una tecla desconocida que el filtro escondía. **Es falso:**
`KEY_NONE` vale `0x2E`, y el decodificador trata como reposo **cualquier** byte
sin el bit `0x40` — `0x17` y `0x27` los dos. Ese cambio habría atribuido a un
canal cada lectura en reposo. Revertido antes de compilar, y el porqué queda en
un comentario del código para que nadie lo reintente.

**El `0x27` sigue sin identificar**, pero ya se sabe que **no es una tecla
pulsada**. Apuntado con los demás cabos sueltos.

### Lo que costó, y por qué

Se persiguió durante horas un fallo de hardware que no existía. Se llegó a
invertir los cables de las patas 3 y 4 —descartando la polaridad—, se pidieron
mediciones con multímetro que no llegaron a hacerse, y se ordenaron hipótesis
sobre la corriente del fototransistor. **Todo eso sobraba.**

Dos cosas lo habrían cortado antes:

1. **El instrumento no se había verificado para la medida que se le pedía.** El
   sniffer estaba validado para escuchar el bus, no para escucharlo *durante*
   una pulsación propia. Nadie comprobó ese caso, y era el único que importaba.
2. **Hubo evidencia física temprana que se subordinó a la captura**: el display
   se despertó justo al disparar el canal 2. Se anotó y se siguió confiando en
   el log.

**Regla que queda:** cuando el mundo físico y la captura discrepen, **el
sospechoso es la captura**.

### Y después, moviendo el escritorio de verdad

Con el canal ya verificado se pasó a usarlo, y salieron **dos hechos que
invalidan supuestos escritos**.

**1. Con 300 ms el escritorio no se mueve.** Se intentó bajar de 84 a 80 cm en
lazo cerrado —pulso, leer la altura del bus, decidir— y **siete pulsos no
movieron ni un centímetro**, aunque el `0x57` aparecía en el bus cada vez. Con
**800 ms** bajó 4 cm en 12 pulsos.

[ADR-023](DECISIONS.md) eligió los 300 ms razonando entre el mínimo de 160 ms y
los 2.2 s del movimiento continuo, **dando por hecho que un pulso que el chip ve
es un pulso que mueve**. No lo es. Corregido en [ADR-027](DECISIONS.md): el ancho
pasa a **800 ms**, todavía 2.75× por debajo del umbral peligroso.

**Ese fue el primer posicionamiento automático del proyecto:** el ESP32 pulsando,
leyendo la altura real y parando solo al llegar a 80.

**2. El mismo botón frena su propio movimiento continuo.** Lo pidió quien tiene
el escritorio, señalando —con razón— que los tiempos ya estaban medidos y que un
pulso largo lo manda solo.

Al revisar la documentación apareció **una contradicción interna que llevaba
semanas ahí**: la tabla de [HARDWARE.md](HARDWARE.md) decía *"sigue hasta que se
pulse **cualquier** botón"* y el texto de la misma medición decía *"solo paró al
pulsar **otra** tecla"*. **Con un solo canal cableado, la diferencia es entre
poder frenar y no poder.**

Se midió **bajando a propósito**: si el freno fallaba, el tope físico de 73 cm
detiene el escritorio. Hacia arriba no habría sido seguro.

| Momento | Altura |
|---|---|
| Pulso largo de 2.8 s, durante | 82 → 79 cm |
| Tras soltar el contacto | **siguió solo**: 78, 77 cm |
| **Toque corto, MISMO canal** | **paró en 76 cm**, 8 s sin moverse |

**El texto era el impreciso; la tabla tenía razón.** Corregido en
[HARDWARE.md](HARDWARE.md) y [SEGURIDAD.md](SEGURIDAD.md), y decidido en
[ADR-028](DECISIONS.md), que **reabre explícitamente** la regla de "evitar que el
movimiento continuo arranque".

⚠️ **Y añade un riesgo que antes no existía:** un cuelgue durante el viaje deja
el escritorio moviéndose sin nada que lo pare. **El watchdog no ayuda** — abre el
canal, y abrir no frena. Por eso el movimiento continuo queda **solo con
supervisión** hasta que haya límites por software.

Después se bajó al tope inferior con un pulso largo: **73 cm**, parado solo.

**Sin captura cruda de estas dos pruebas.** Se registró la traza de alturas, no
el volcado del bus, porque el guion de control leía el puerto en vivo. Es una
carencia respecto a la [política](POLITICA_DOCUMENTACION.md) y se anota como tal:
las cifras de arriba salen de la traza, no de un log crudo revisable.

### Lo que sí era real de todo aquello

- El **fallo de recepción del puerto serie** ([ADR-026](DECISIONS.md)) existió y
  sigue sin explicarse. La regla de mandar `h` antes de cada prueba se mantiene.
- El cable **estuvo conectado al canal 1** buena parte de la tarde mientras se
  disparaba el canal 2, cosa que se descubrió al probar los cuatro canales
  seguidos. Un segundo motivo, independiente, para que no respondiera.

---

## 2026-08-21 — Primer pulsador soldado: bajar al canal 2

**Objetivo de la sesión:** paso 3 de la fase 3 — cablear un solo canal a un
pulsador del mando. Se cableó **bajar**, al **canal 2 (GPIO 26)**.

⚠️ **Corrección dentro de la propia sesión.** Esta entrada se escribió primero
diciendo **subir**, porque así se reportó al principio. Es falso: el pulsador
soldado es el de **bajar**, confirmado por quien lo soldó tras probarlo a mano.
Se corrige aquí y en [PLAN.md](PLAN.md). **Consecuencia práctica: el código
esperado en la comprobación es `0x57`, no `0x47`** — que es justo lo contrario
de lo que decía. Si no se hubiera detectado, un canal bien soldado habría
parecido un error, o al revés.

**Estado del montaje al empezar:** los cuatro PC817 en protoboard, verificados
el 2026-08-20, con el lado de salida sin conectar a nada. Sonda montada y mando
funcionando con normalidad.

### Qué se hizo

Soldados dos hilos desde el pulsador de **bajar** a las patas **3 y 4** del
PC817 del canal 2 (lado fototransistor, no el LED).

Después, dos comprobaciones:

| Comprobación | Resultado | Cómo se midió |
|---|---|---|
| Los dos hilos soldados, pulsador en reposo | **abierto** | Multímetro, modo continuidad, mando suelto |
| Los dos hilos soldados, apretando bajar a mano | **cierra** | Ídem |
| El pulsador de bajar, a mano, con el escritorio conectado | **funciona con normalidad** | Observado: el escritorio baja |

**El modo continuidad es válido aquí** — al contrario que en la medición del
2026-08-20. El pulsador es un contacto mecánico que cae a ~0 Ω y el zumbador lo
detecta; lo que no detectaba era el fototransistor saturado, que se queda en
unos cientos de ohmios. Son dos medidas distintas sobre dos elementos distintos.

**Verificado:** soldar no rompió el pulsador ni alteró el funcionamiento del
mando. Las patas cogidas son el par en diagonal, el que cierra al apretar.

### Qué se espera al comprobar

Escrito antes de probar:

- Al mandar `2` por el puerto a `desk_sniffer`, la caja de control ve una
  pulsación de **bajar**: `<== CHANNEL 2 answered with 0x57`.
- El escritorio da un toque hacia abajo — un pulso de 300 ms está muy por
  debajo de los 2.2 s que arrancan el movimiento continuo.
- Si la respuesta es `0x47`, `0x6F` o `0x67`, está soldado al pulsador
  equivocado. Si no responde nada, lo más probable es la polaridad del
  fototransistor: cambiar los dos hilos de sitio.

**El bus es el árbitro, no la etiqueta.** Esta comprobación vale precisamente
porque no depende de que nadie recuerde bien qué botón es cuál — el código que
imprima el sniffer lo dice sin ambigüedad.

### Resultados

**La soldadura está bien** (verificado, tabla arriba). **El disparo desde el
ESP32 está pendiente** — todavía no se ha mandado `2` por el puerto.

### Estado del montaje al terminar

- Canal 2 (GPIO 26): patas 3 y 4 soldadas al pulsador de **bajar**. Soldadura
  **verificada** por continuidad y con el pulsador funcionando a mano.
- Canales 1, 3 y 4: lado de salida suelto.
- Escritorio conectado a la corriente.
- Resto sin cambios respecto al 2026-08-20.

### Siguiente paso

Disparar el canal 2 desde `desk_sniffer`: mandar `2` por el puerto, sin tocar el
mando, y anotar el código que responda la caja de control. Se espera `0x57`.

---

## 2026-08-20 — Primer canal de accionamiento verificado

**Objetivo:** comprobar que un canal PC817 se enciende solo cuando el firmware
lo ordena, y nunca al arrancar ni al reiniciar. Paso 1 de la fase 3.

**Estado del montaje:** ESP32 por USB al Mac, **escritorio desenchufado de la
corriente**. La sonda sigue conectada al mando pero sin bus vivo. El PC817 en
protoboard con su lado de salida **conectado a nada**, solo al multimetro.

### Que se esperaba

Que los cuatro pines de control leyeran 0 al arrancar, que el canal estuviera
abierto en reposo y con el chip en reset, y que condujera solo durante los
300 ms del pulso ordenado.

### Resultado — todo correcto

| Comprobacion | Resultado |
|---|---|
| Nivel de los cuatro pines al arrancar | **0** los cuatro |
| Con EN apretado, chip en reset | **OL** — abierto |
| En reposo tras arrancar | **OL** — abierto |
| Durante el pulso | **152 Ω** — conduce |
| Duracion del pulso | **300 ms exactos**, medido 40 veces seguidas |

**Los 152 Ω son suficientes.** El pulsador real cierra un camino que ya lleva
2 kΩ en serie con ~90 µA. Añadir 152 Ω sube la resistencia un 7% y hace caer
**13 mV**. Para el chip es un boton pulsado.

**Esto sostiene [ADR-024](DECISIONS.md):** el canal solo conduce cuando el
firmware lo ordena. Un reinicio deja los pines en alta impedancia y el canal
abierto, que es exactamente lo que el watchdog necesita para proteger.

### Dos cosas que costaron mas que la medicion

**El modo continuidad del multimetro enganaba.** No pitaba con el canal
conduciendo, y parecia que el circuito no funcionaba. Un fototransistor saturado
se queda en unos cientos de ohmios, y los zumbadores solo pitan por debajo de 30
o 50. **Habia que medir en ohmios, no en continuidad.** Es el mismo patron de
siempre: el instrumento diciendo algo que no era sobre el mundo.

**El Arduino IDE cargaba siempre el sketch equivocado.** Cuatro intentos
seguidos acabaron con `desk_sniffer` en la placa en vez del sketch de prueba, y
no hubo forma de que el IDE cargara el correcto.

Resuelto pasando a **arduino-cli**, que viene dentro del propio Arduino IDE:

```
CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
"$CLI" compile --fqbn esp32:esp32:esp32 firmware/test_output_channels
"$CLI" upload -p /dev/cu.usbserial-0001 --fqbn esp32:esp32:esp32 firmware/test_output_channels
```

Compila y carga sin ambiguedad, y de paso permite mandar comandos al sketch
por el puerto desde un script — que es como se dispararon los 40 pulsos
encadenados. **A partir de ahora se carga asi.**

### Los cuatro canales montados y verificados

Montados los otros tres PC817 y comprobados uno a uno, 40 pulsos encadenados
por canal:

| Canal | Pin | Resultado |
|---|---|---|
| 1 | GPIO 27 | ✅ OL en reposo, ~150 Ω al pulsar |
| 2 | GPIO 26 | ✅ |
| 3 | GPIO 25 | ✅ |
| 4 | GPIO 33 | ✅ |

**Con los cuatro optoacopladores montados, los cuatro pines siguen leyendo 0 al
arrancar.** Ninguno se activa solo, que es lo que sostiene [ADR-024](DECISIONS.md).

Todos los pulsos, 160 en total, salieron de **300 ms exactos**.

**Incidencia menor:** a mitad del montaje el puerto `/dev/cu.usbserial-0001`
desaparecio del sistema — el cable USB se habia soltado al mover la protoboard.
Se detecta porque `ls /dev/cu.*` deja de listarlo. Conviene comprobarlo antes de
buscar fallos en otro sitio.

### Los pulsadores del mando, ya identificados

Confirmado por quien lo maneja: los cinco pulsadores estan localizados y **el
reset identificado**. Paso 2 cerrado sin trabajo adicional.

### El accionamiento se integra en el sniffer

Escrito lo que hacia falta antes de soldar nada. El problema era que
`desk_sniffer` y `test_output_channels` **no pueden correr a la vez**, y para
comprobar que un canal quedo soldado al boton correcto hace falta justo eso:
disparar el canal y leer en el bus que codigo de tecla responde.

Sin eso se suelda a ciegas y el error no aparece hasta mucho despues.

**Decision: los dos en un solo sketch, no en dos.** Dos copias del decodificador
acabarian divergiendo — es el mismo riesgo que ya se anoto con
`test_capture_ceiling`, que copia el ISR del sniffer y solo sirve mientras los
dos coincidan.

`desk_sniffer` pasa a hacer las dos cosas:

- **El bus sigue siendo de solo lectura para siempre.** Sus dos pines son
  entrada de `setup()` al apagado. [ADR-011](DECISIONS.md) intacto.
- **Los cuatro canales de accionamiento van por pines aparte**, aislados
  galvanicamente y en paralelo a los pulsadores, que es justo la ruta que
  ADR-011 prescribe.
- Los pines se ponen bajos **como primera instruccion de `setup()`**, antes que
  nada mas, por lo que exige [ADR-024](DECISIONS.md).
- Comandos `1` `2` `3` `4` para disparar un canal 300 ms.
- **Y lo que da valor a todo esto:** tras el pulso vigila el bus 1.5 s, y cuando
  aparece una tecla pulsada imprime `<== CHANNEL n answered with 0xNN`. Un canal
  soldado en el boton equivocado se delata en el acto.

Compilado con la toolchain real: 21% de programa, 9% de RAM.

### Siguiente

**Soldar el primer canal** a un pulsador y comprobarlo con ese mensaje. Tabla de
codigos en [PROTOCOLO.md](PROTOCOLO.md): subir `0x47`, bajar `0x57`, memorias
`0x6F` y `0x67`.

---

## 2026-08-06 — Se suelda, se monta la sonda, y el protocolo queda descifrado

**Objetivo:** montar la sonda y capturar por primera vez.

**Resultado:** conseguido, y bastante más. **La fase 2 queda cerrada.**

**Estado del montaje al empezar:** nada soldado. ESP32 con
`test_capture_ceiling` cargado del 2026-08-03.

**Fase 3 abierta al final de la sesión**, con la decisión de seguridad cerrada
(ADR-023 y ADR-024) y sin nada cableado todavía. Los PC817 comprobados y sueltos.

**Estado del montaje al terminar — TODO MONTADO Y FUNCIONANDO:**

- **Tres hilos soldados** al conector JST del mando, con el conector original
  puesto. El amarillo **no** se soldó. Verificado: 11 MΩ entre rojo y verde, sin
  puentes de estaño.
- **Sonda montada en protoboard**, dos canales de 9.1 k + 7.4 k arriba y 27 kΩ
  abajo ([ADR-022](DECISIONS.md)). Verificada con el cable azul fuera: 36 kΩ por
  canal.
- **Conectada al ESP32**: P18 = CLK (rojo), P4 = DIO (verde), GND común.
- **ESP32 alimentado por USB** desde el Mac, con la versión de **muestreo por
  ráfagas** de `desk_sniffer` cargada — no la de interrupciones, que no servía.
- **El escritorio funciona con normalidad** con todo conectado. El mando
  responde igual que siempre.
- Bus a **4.7 V**, nodo del GPIO a **2.9 V**, medidos con todo en marcha.

**Al retomar:** si algo se desconectó por el camino, el orden de
[ADR-019](DECISIONS.md) es **USB primero, hilos del bus después**; al desmontar,
al revés. Y el divisor solo se comprueba con el cable azul fuera, o el mando
mete un camino paralelo de ~34 kΩ y el número no significa nada.

### Componentes recibidos y medidos

Llegan las resistencias. Medidas antes de usarse, como exige
[ADR-015](DECISIONS.md):

- **27 kΩ: entre 26.79 y 27.01 kΩ** pieza a pieza. Con las 9.1 kΩ del cajón dan
  bus a 3.99 V y GPIO a 2.98–2.99 V — clavado en [ADR-016](DECISIONS.md). Los
  extremos del lote se separan 10 mV en el GPIO, así que da igual qué dos se usen.
- 10 kΩ y 330 Ω, para la fase 3.

También llegan los optoacopladores. Sin evaluar todavía.

### Soldadas las tres derivaciones ✅

Tres hilos a las juntas del conector JST del mando, con el conector original
puesto. **El amarillo no se soldó**, como manda el diseño.

**Verificación de las soldaduras — pasada:** medido **rojo ↔ verde con todo
desconectado y el escritorio sin corriente: ~11 MΩ.** A efectos prácticos,
circuito abierto.

Eso confirma dos cosas de una vez:

1. **No hay puente de estaño** entre pines vecinos del conector, que es el fallo
   que no se ve a simple vista y que HARDWARE.md obliga a descartar.
2. **CLK y DIO son señales independientes**, lo que el paso B del plan daba como
   comprobación opcional. Con el chip sin alimentar solo quedan fugas por sus
   estructuras de protección, y 11 MΩ es justo eso.

### El divisor está bien: lo que sobraba era el mando sin alimentar

**Resuelto, y el montaje es correcto.** La medición que lo aclaró todo fue
separar el cable azul:

| Canal | Sin el cable azul | Con el cable azul |
|---|---|---|
| Verde | **36.04 kΩ** ✅ | 17.34 kΩ |
| Rojo | **35.5 kΩ** ✅ | 17.34 kΩ |

**El divisor solo mide lo que tiene que medir.** Conectar la masa del mando es lo
que parte el valor por la mitad, y no es un fallo: es física esperada.

**Mecanismo.** Rojo y azul están unidos por dentro del mando — medido antes:
**rojo ↔ azul = 24 kΩ**. Con el escritorio desenchufado el chip está sin
alimentar, su pull-up interno cuelga de un VDD que está a 0 V, y eso deja un
camino desde CLK hasta masa. Al conectar el azul, la entrada ve **dos caminos en
paralelo**:

```
divisor propio:   35.5 kΩ
por el mando:     ~34 kΩ
en paralelo:      17.3 kΩ   ← lo medido
```

**Y explica la primera lectura de todas.** Aquella tenía además el ESP32
conectado, cuyos GPIO llevan pull-down interno de ~45 kΩ:

```
17.34 ∥ 45 = 12.5 kΩ      (se midió 12.8)
```

**Las tres lecturas que se persiguieron toda la sesión eran correctas.** Ninguna
indicaba un fallo. Cada una era el divisor más lo que hubiera conectado.

**Esto desaparece al encender el escritorio:** con la caja alimentada, VDD sube a
5 V y ese "camino a masa" pasa a ser lo que realmente es, el pull-up hacia 5 V
que el diseño ya contempla.

**Corrección de una corrección.** Antes de llegar aquí se escribió en esta misma
entrada que la causa era *"un número mal transcrito"*. **Eso también era falso.**
Los números estaban bien desde el principio; lo que faltaba era preguntar **en
qué condiciones** se tomaba cada uno.

**Lección, y aplica igual a las capturas del bus que vienen:** se construyeron
**cuatro** hipótesis consecutivas —resistencias equivocadas, canales en paralelo,
entradas cortocircuitadas, error de transcripción— y las cuatro cayeron. Todas
compartían el mismo defecto: **daban por supuesto el estado del montaje en el
momento de medir.** Una medición sin sus condiciones no es un dato, y la regla 1
de la [política](POLITICA_DOCUMENTACION.md) ya lo exige para la bitácora —
"cómo se midió, contra qué referencia"— pero no se estaba aplicando al preguntar.

**Qué habría ahorrado tiempo:** preguntar desde el principio *"¿qué hay conectado
exactamente cuando tomas esa lectura?"* en vez de proponer una causa nueva cada
vez.

<details>
<summary>Las cuatro hipótesis descartadas, conservadas como registro</summary>

Montado el divisor, las medidas no dan lo que deberían. **No se conectó nada al
escritorio**, precisamente porque las comprobaciones previas lo impidieron.

| Medida | Esperado | Obtenido |
|---|---|---|
| Entrada ↔ P18 | 9.1 kΩ | **8.9 kΩ** ✅ |
| Entrada ↔ P4 | 9.1 kΩ | **8.9 kΩ** ✅ |
| Entrada rojo ↔ masa | 36.1 kΩ | **12.8 kΩ** ❌ |
| Entrada verde ↔ masa | 36.1 kΩ | **13.2 kΩ** ❌ |
| Ídem, con los jumpers al ESP32 fuera | 36.1 kΩ | **17.3 kΩ** ❌ |

**Las resistencias no son el problema:** sacadas de la protoboard y medidas en
serie en la mano, **dan los 36 kΩ correctos**. El fallo está en cómo están
puestas en la protoboard.

**Lo que se descartó por medición:**

- Que las piezas estuvieran mal — dan 36 kΩ sueltas en serie.
- Que rojo y verde estuvieran cortocircuitados en la entrada — no dan
  continuidad.
- Que el lado del mando estuviera mal — 11 MΩ entre las derivaciones.

**Lo que habría pasado de haberlo conectado:** con 12.8 kΩ de carga contra el
pull-up interno de 9.1 kΩ, el bus habría caído a **2.92 V**, por debajo del VIH
de 3.5 V del chip. El mando habría dejado de funcionar, y al GPIO le habrían
llegado 0.9 V, así que el sniffer tampoco habría visto nada. **Dos fallos a la
vez, con el síntoma apuntando a las soldaduras del mando en vez de a la
protoboard** — exactamente el modo de fallo de [ADR-005](DECISIONS.md), evitado
esta vez por medición en vez de por cálculo.

</details>

**Estado: los dos canales verificados** — 36.04 kΩ el verde y 35.5 kΩ el rojo,
medidos con el cable azul fuera, que es la única forma de ver el divisor solo.

**Regla práctica que sale de aquí, para cualquier medición futura del divisor:**
para comprobar la sonda hay que hacerlo **con el cable azul desconectado**. Con
el azul puesto y el escritorio apagado, el mando aporta un camino paralelo de
~34 kΩ y el número no significa nada.

### La comprobación del cociente destapa un pull-up no documentado

Con el escritorio encendido y quieto, siguiendo el orden de
[ADR-019](DECISIONS.md) —USB primero, hilos después—:

| | Medido |
|---|---|
| Bus sin sonda (rojo↔azul y verde↔azul) | **4.9 V** |
| Rojo ↔ verde | 0 V — las dos en reposo alto, al mismo potencial |
| Bus con sonda 9.1 k / 27 k | **4.6 V** |
| **Cociente** | **0.94** |

Lo esperado era 0.80. Un cociente **más alto** significa que el divisor apenas
carga el bus, y eso solo puede venir de un pull-up más fuerte:

```
R_pullup = 36.1 kΩ × (1 − 0.94) / 0.94 ≈ 2.4 kΩ
```

**El mando lleva pull-ups externos no documentados**, casi cuatro veces más
fuertes que los 9.1 kΩ internos del datasheet. Es el primer parámetro del
proyecto que no viene de una hoja de fabricante sino de medir el bus vivo.

**Y con ello, un error de razonamiento que llevaba semanas escrito.** ADR-013
había contemplado esta posibilidad y la despachó con *"el pull-up solo sería más
fuerte, dimensionar para 9.1 kΩ es seguro"*. Falso a medias: un pull-up fuerte
juega a favor del bus y **en contra del GPIO**. Medido en el nodo: **3.5 V**,
contra un máximo absoluto de 3.6 V. **Margen del 2%.**

Corregido en el momento añadiendo una 7.4 kΩ en serie con cada 9.1 kΩ
([ADR-022](DECISIONS.md)). Resultado medido: **bus 4.7 V, GPIO 2.9 V**, ambos
centrados. El mando sigue funcionando con normalidad.

Sin la comprobación del cociente —que hace tres días parecía una formalidad— el
montaje habría quedado capturando durante horas a un 2% de un máximo absoluto sin
que nadie lo supiera.

### Primera captura real del bus ✅

**El sniffer lee el bus.** Aparecen comandos que coinciden con el datasheet del
AiP650E: `48` (control de display), `6A`/`6C`/`6E` (dígitos) y `4F` (lectura de
teclado, decodificado como *KEY released/idle*). Alguna transacción sale perfecta
de dos bytes.

**Pero el encuadre falla:** 40 de 41 transacciones malformadas, casi todas
corriéndose hasta 8 bytes. Estadísticas con el volcado crudo apagado:

```
  edges captured : 18544
  edges dropped  : 0
  transactions   : 41 (40 malformed, 13 ended by repeated START)
  fastest clock  : 5125 ns period (~195 kHz)
```

**Hipótesis descartada:** que la impresión por serie saturase y desbordase la
cola. `dropped` es cero.

**Dos hipótesis vivas, y llevan a soluciones distintas:**

1. **El bus corre de verdad a 195 kHz.** Está por encima del techo de captura
   medido el 2026-08-03 (125 kHz limpio, pérdida silenciosa desde 150). Tocaría
   el plan B del periférico RMT que [PLAN.md](PLAN.md) ya declara.
2. **Cada flanco real se cuenta dos veces.** El divisor redondea los flancos y
   una subida lenta con ruido cruza el umbral del GPIO más de una vez. El bus
   real sería la mitad, ~97 kHz, dentro del techo. Tocaría el buffer Schmitt
   declarado en [ADR-018](DECISIONS.md).

**Lo que apunta a la segunda:** 18.544 flancos entre 41 transacciones son **452
flancos por transacción**. Una transacción de 8 bytes son 72 pulsos de reloj, o
sea unos 180 flancos como máximo — y muchas tenían menos de 8 bytes. **Se están
capturando entre dos y tres veces más flancos de los que el protocolo puede
generar.**

**No se eligió por intuición.** Se añadió al sniffer un **histograma de
intervalos entre flancos**, calculado en `loop()` y nunca en el ISR, para no
agravar justo el efecto que se investiga. Un bus real no puede producir dos
flancos separados 200 ns; un umbral que rebota, sí.

### El histograma decide: el bus es rápido de verdad

```
< 0.25 us :      0  ( 0%)
< 0.5  us :      0  ( 0%)
< 5    us :   8963  (44%)
```

**Cero flancos por debajo de medio microsegundo.** No hay dobles. **La hipótesis
del eco era falsa** y el montaje está eléctricamente limpio: el divisor no
ensucia nada.

Queda la otra: el bus corre de verdad a **~202 kHz**, por encima del techo medido
el 2026-08-03 (125 kHz limpio, pérdida silenciosa desde 150). Y esa cifra es un
**suelo**: si se pierden flancos de reloj, el periodo medido puede ser un
múltiplo del real.

### Segundo capturador: muestreo por ráfagas

Reescrita la parte de captura del sniffer. En vez de reaccionar a cada flanco
con una interrupción, **graba las dos líneas a intervalo fijo y decodifica
después**:

- **4 MHz de muestreo**, unas 20 muestras por cada estado de un bus de 200 kHz.
  Nada puede llegar "tarde" porque nada hay que atender a tiempo.
- **Ráfagas de 2 ms** con las interrupciones apagadas. Más largo dejaría el
  puerto serie sin atender; 2 ms cubre cinco veces la transacción más larga vista.
- **Se arma esperando a que el bus despierte.** Grabar a ciegas fallaría cuatro
  de cada cinco veces, porque el tráfico llega a ráfagas cada ~43 ms.
- **El decodificador de protocolo no se tocó.** Las muestras se reproducen por
  el mismo `feed()` de antes, así que si ahora funciona es que el problema era
  la captura y no la interpretación.

**Resultado inmediato:** transacciones de dos bytes exactos, todas decodificadas.
`MALFORMED` desaparece.

```
S 68a 00a P  | DIG1    S 6Aa 00a P  | DIG2
S 6Ca 00a P  | DIG3    S 6Ea 00a P  | DIG4
S 4Fa 27a P  | KEY released/idle
```

**El decodificador llevaba semanas bien escrito.** Lo único que fallaba era
cómo se le daban los datos.

### 🎯 EL PROTOCOLO QUEDA DESCIFRADO

Dos capturas guardadas en [capturas/](capturas/), con su cabecera de contexto:
`2026-08-06-movimiento-subir.log` y `2026-08-06-pulsadores.log`.

**La altura se lee del bus, verificada contra la pantalla del mando:**

| Acción | Lo que se vio en el mando | Lo que leyó el sniffer |
|---|---|---|
| Subir manual | 86 | `086` ✅ |
| Bajar manual | 77 | `077` ✅ |
| Memoria alta | 117 | `117` ✅ |
| Memoria baja | 80 | `080` ✅ |

El caso de **117 es el que cierra la verificación**: confirma el dígito de las
centenas, que ninguna captura previa había alcanzado.

**La altura se refresca DURANTE el movimiento.** Era el riesgo que podía tumbar
el proyecto entero, escrito en PLAN.md como *"si no, el lazo cerrado no funciona
y hay que replantear"*. Se actualiza **cada centímetro, a ~1.2 s**, sin esperar a
llegar. **El lazo cerrado es viable y el requisito real de [ADR-001](DECISIONS.md)
queda confirmado sobre hardware.**

**Velocidad medida: 8.5 mm/s**, idéntica en los dos sentidos — 39 cm en 45.9 s
subiendo, 36 cm en 42.5 s bajando. Los primeros centímetros tardan 2.5–3 s cada
uno: es la rampa del motor, y es exactamente por lo que contar segundos nunca
habría funcionado.

**Teclas mapeadas**, pulsando cada botón por separado:

| Código | Botón |
|---|---|
| `0x47` | Subir |
| `0x57` | Bajar |
| `0x6F` | Memoria de 117 cm |
| `0x67` | Memoria de 80 cm |
| — | Reset: **no se pulsó, y no se pulsará** ([ADR-008](DECISIONS.md)) |

**Hallazgo no buscado, y valioso: al pulsar una memoria el display parpadea la
altura de DESTINO** antes de moverse, y solo después cuenta la altura real. El
ESP32 podrá saber a dónde va el escritorio en el instante en que alguien pulsa,
en vez de perseguir el movimiento. Para la fase 4 cambia el diseño a mejor.

**Diferencia real contra el datasheet:** en reposo el teclado vale `0x27`, no el
`0x2E` documentado. Lo que sí se cumple sin excepción es el bit 6 a cero en
reposo, que es lo que importa para detectar pulsaciones. Anotado en
[PROTOCOLO.md](PROTOCOLO.md).

**Dato nuevo para [ADR-010](DECISIONS.md):** un toque de memoria produce **una
sola lectura de teclado**, o sea que dura menos de los ~200 ms del ciclo. Primera
medida real de cuánto es "corto" en este mando.

### El umbral de grabar preset: 3.0 s — desbloquea la fase 3

Era el último número que faltaba, y el que **bloqueaba el cableado de M1/M2**.

**Se midió sin arriesgar nada.** En vez de sacrificar un preset, se llevó el
escritorio a 080 y se mantuvo pulsada la memoria **que ya valía 080**: si graba,
graba el mismo valor que tenía. Riesgo cero y misma medición.

Y se contó **sobre el bus**, no con cronómetro. La caja lee el teclado cada
~200 ms, así que esa es la resolución — mejor que un pulso humano.

```
16.781 s   primera lectura con la tecla pulsada (0x67)
   ...     15 lecturas seguidas, display fijo en '080'
19.778 s   el display se apaga por primera vez  <- HA GRABADO
20.378 s   parpadeo
20.977 s   parpadeo
21.576 s   parpadeo
22.175 s   parpadeo
```

**2.997 s del inicio de la pulsación al primer parpadeo.** La confirmación son
cinco parpadeos cada 0.6 s, y siguen aunque se suelte. Coincide con lo que la
persona observó: *"parpadeó como 4 o 5 veces, pisado como por 3 seg"*.

**La ventana de trabajo queda con quince veces de margen:**

| | Duración |
|---|---|
| Recuperar un preset (toque) | < 0.2 s |
| Grabar un preset (mantener) | **3.0 s** |

Bastante más holgado de lo que ADR-010 temía. Para destruir un preset, un relé
pegado tendría que quedarse cerrado **tres segundos completos**, no unas décimas.

**Diseño que sale de aquí:** pulsos de 200–300 ms para recuperar, con el
temporizador de hardware cortando a 500 ms. Seis veces por debajo del umbral y
por encima del mínimo de 160 ms que exige el chip. Anotado en
[HARDWARE.md](HARDWARE.md).

---

### La decisión de seguridad de la fase 3, cerrada — y casi al revés de como se planteó

Quedaba pendiente si subir y bajar se acotan por hardware. Se planteó como
*"añadir un vigilante que abra el canal si el firmware se cuelga"*. **Dos
mediciones dieron la vuelta al problema.**

**Primera: subir y bajar tienen dos regímenes, no uno.**

| | Qué hace | ¿Se para solo? |
|---|---|---|
| Toque corto | Mueve ~1 cm | **Sí** |
| Mantener y soltar | Arranca movimiento continuo | **No.** Sigue hasta pulsar otro botón |

Confirmado sobre la captura de pulsadores: tras soltar, el escritorio siguió
**5 cm en 5 s** subiendo y **6 cm en 6.6 s** bajando. En los dos casos paró
únicamente al pulsar otra tecla.

**Eso invalidó las dos primeras propuestas.** Tanto un latido que abriera el
canal por ausencia de señal como un one-shot que cortara el cierre protegen
contra un fallo que no existe: **si el movimiento continuo ya arrancó, abrir el
contacto no lo para.** Para detenerlo hay que *cerrar* un contacto, y un
circuito que actúa por ausencia de señal no puede hacer eso.

*La corrección vino de quien maneja el escritorio, no del análisis: la
descripción que se estaba usando —"para cuando sueltas"— contradecía lo que ya
decía HARDWARE.md. Los datos para desmentirlo llevaban horas capturados.*

**Segunda: el umbral que separa toque de continuo.** Medido pulsando cada vez
más largo, en [2026-08-06-umbral-toque-vs-continuo.log](capturas/2026-08-06-umbral-toque-vs-continuo.log):

| Duración | Resultado |
|---|---|
| 200 ms · 1.0 s · 1.6 s · 1.8 s | Toque |
| **2.2 s** | Toque — la más larga que se paró sola |
| **2.6 s** | **Continuo** — la más corta que se disparó |

Y el de grabar preset es **3.0 s**. **Los dos rondan los 2–3 segundos:** la caja
parece tener un único concepto de "pulsación larga" para los cuatro botones.

**Decisión ([ADR-023](DECISIONS.md)): limitador de ancho de pulso a 300 ms,
idéntico en los cuatro canales.** El firmware solo emite toques y el hardware
garantiza que ninguno dure más. Siete veces por debajo del umbral peligroso y
casi el doble del mínimo de 160 ms que el chip exige.

Un solo mecanismo en vez de dos protecciones distintas, y **los dos modos de
fallo peligrosos desaparecen a la vez**: un contacto pegado no puede arrancar
movimiento continuo ni sobrescribir un preset.

**De propina, observación de quien pulsaba:** el display **parpadea al arrancar
movimiento continuo**. Confirmado en el bus — 0 parpadeos tras el toque de 2.2 s,
2 y 3 tras los continuos. Da al firmware una forma de detectar un movimiento que
no pidió. Queda como refuerzo por software, no como la protección.

### Los topes físicos: 73 a 118 cm

Medición propuesta por quien maneja el escritorio, no por el análisis, y era un
hueco real: nadie había mirado qué pasa al llegar al final del recorrido.
Captura [2026-08-06-topes-fisicos.log](capturas/2026-08-06-topes-fisicos.log).

**Rango real: 73 a 118 cm.** 45 cm de recorrido. Hasta ahora solo se conocía de
77 a 117, que es donde se había estado, no dónde están los límites.

**Al topar no pasa nada especial**, y eso es buena noticia:

- El display **sigue mostrando un número**. Cero códigos de segmento
  desconocidos en toda la captura.
- **No parpadea** al topar.
- **Ningún comando nuevo** en el bus.
- Apenas frena: mantiene ~1.2 s por centímetro; el último de arriba tardó 1.4 s.

**Consecuencia para el firmware:** llegar al tope es **indistinguible de estar
parado**. La única señal es que la altura deja de cambiar aunque se sigan
mandando toques. Ese tendrá que ser el criterio para no encadenar toques
indefinidamente contra un límite.

**Corrección de una interpretación previa:** se había supuesto que el parpadeo
del display podía marcar la llegada al tope. **No.** Marca que **arrancó un
movimiento continuo**. Confirmado en dos capturas: 0 parpadeos tras un toque que
se paró solo, 2 y 3 tras los continuos, y **ninguno al topar**.

**Dos hallazgos laterales:**

- **El comando de control `0x48` siempre lleva el mismo byte, `0x21`** —
  `display=ON, sleep=no` — en las 17 apariciones de todas las capturas. La caja
  **nunca ha mandado el bit de sueño**. Con el escritorio quieto apaga la
  pantalla escribiendo ceros, pero no duerme el chip.
- **Un comando `0x90 42` que no está en el datasheet**, presente en las cinco
  capturas, una a cinco veces cada una, siempre con ACK. Se cuela al principio de
  un ciclo de refresco. No impide nada. **Sin explicar**, y anotado como tal en
  [PROTOCOLO.md](PROTOCOLO.md).

### Cuarta vez que un contador mide la herramienta y no el mundo

Al revisar las capturas apareció un **0.7–1 % de transacciones malformadas**,
cuando se había afirmado que el muestreo por ráfagas las eliminaba. **Las 171
tenían exactamente cero bytes.**

No era tráfico roto: era el propio capturador. Entre ráfaga y ráfaga el bus se
mueve sin que nadie mire, y el decodificador —cuyo estado es global— comparaba el
primer cambio de la ráfaga nueva contra un nivel de milisegundos antes,
inventando un START en la costura. Corregido resembrando el decodificador en cada
ráfaga, con un contador aparte (`cut by burst`) para no esconder lo que se
descarta.

**Y es la cuarta vez en la misma sesión.** El patrón completo:

| Contador | Qué parecía medir | Qué medía |
|---|---|---|
| `dropped` | Si se pierden flancos | Solo los que llegan al ISR. Los que nunca lo disparan no cuentan |
| `RESULT: clean up to 400 kHz` | La capacidad de captura | Nada: aprobaba con el cable desconectado |
| `malformed` al 1 % | Errores del bus | Las costuras entre nuestras propias ráfagas |
| Un hueco de 100 s sin tráfico | Que el bus se durmió | **Que el Mac se durmió** |

El último lo identificó quien estaba delante, no el análisis. **Regla que sale de
aquí, y que vale para toda la fase 3: antes de creerse un contador, preguntarse
qué mide cuando no hay nada que medir.**

### El bus nunca se calla — ADR-012 acotado

Quince minutos con el escritorio quieto, el volcado crudo apagado y el Mac
forzado despierto con `caffeinate`. Resultado en
[2026-08-06-reposo-largo-2.log](capturas/2026-08-06-reposo-largo-2.log):

```
bursts         : 4505 recorded, 0 armed on a quiet bus
transactions   : 22731 (175 malformed)
cut by burst   : 0
late samples   : 0
digit writes   : 18020        key reads : 4505
ctrl writes    : 31
```

**`0 armed on a quiet bus` tras 4505 intentos. El bus no se calla nunca.** A los
pocos segundos el display se apaga escribiendo `0x00` en los cuatro dígitos, pero
el refresco de 200 ms no se detiene, y el comando de control siempre dice
`sleep=no`. **Son dos cosas distintas y solo ocurre la primera.**

Eso acota mucho [ADR-012](DECISIONS.md) sin anularlo: la caducidad de la altura
sigue haciendo falta —la caja puede reiniciarse, un cable soltarse— pero **el
sueño del chip deja de ser un escenario esperado**. Y aparece una señal mejor de
la que el ADR preveía: como el bus sigue vivo con la pantalla en blanco, el ESP32
puede distinguir *"no hay altura que mostrar"* de *"no hay bus"*.

**Integridad perfecta:** 18.020 escrituras de dígito ÷ 4 = **4.505 ciclos =
4.505 lecturas de teclado**. Ni un ciclo incompleto ni uno perdido, en quince
minutos.

**Sobre las 175 malformadas (0.77 %), que la resiembra no eliminó.** No son
periódicas —separación de 0.2 a 31 s, mediana 3.6 s— así que no son un artefacto
sistemático de las costuras, y `cut by burst` es cero. Y sobre todo: **la cuenta
de ciclos cuadra exacta, así que no son transacciones corrompidas sino
transacciones de más** — pares `S P` sin bytes que se cuelan entre las buenas.

**Hipótesis, sin verificar:** a 4 MHz cada muestra son 250 ns. Si DIO cambia
dentro de esa ventana respecto al flanco de bajada de CLK, se ve un cambio de
DIO con CLK todavía alto, que por definición es un START o un STOP. Sería
resolución de muestreo, no protocolo.

**Se deja como está, documentado.** No pierde datos y no bloquea nada. Si en la
fase 4 estorba, la vía es exigir que CLK lleve varias muestras estable antes de
declarar START o STOP. **Mientras tanto, el suelo de ruido de `malformed` es
~0.8 %, no cero**, y hay que interpretarlo con eso en mente.

### Qué queda abierto al cerrar la sesión

1. **Diseñar el limitador de ancho de pulso de 300 ms.** La decisión ya está
   tomada ([ADR-023](DECISIONS.md)); falta elegir con qué se implementa y qué
   hace falta comprar.
2. **Cuál de las dos memorias es M1 y cuál M2.** Se sabe qué código va con qué
   preset —`0x6F` con 117 cm y `0x67` con 80 cm— pero no cuál botón físico es
   cuál. Trivial de resolver cuando haga falta cablearlos.
3. **Si alguna función usa combinación de teclas.** El chip lo soporta y nada lo
   sugiere todavía, pero no se ha probado a propósito.
4. **Qué es el comando `0x90 42`.** Presente en las cinco capturas, ausente del
   datasheet, sin efecto observable. Anotado en [PROTOCOLO.md](PROTOCOLO.md).
5. **El suelo de ruido de `malformed` es ~0.8 %, no cero.** Son transacciones de
   más, no pérdidas — la cuenta de ciclos cuadra exacta. Documentado en
   [firmware/README.md](../firmware/README.md).

### Lección de método, que es la que más va a servir

Durante la depuración del divisor se construyeron **cuatro hipótesis de fallo
consecutivas** —resistencias equivocadas, canales en paralelo, entradas
cortocircuitadas, error de transcripción— y **las cuatro eran falsas**. La causa
real era que el mando, sin alimentar, aportaba un camino paralelo a masa.

Todas compartían el mismo defecto: **daban por supuesto el estado del montaje en
el momento de medir**. La pregunta barata era *"¿qué hay conectado exactamente
cuando tomas esa lectura?"*, y se tardó cuatro rondas en hacerla.

Lo mismo se repitió con el encuadre de las transacciones: se propuso primero la
saturación del puerto serie (falsa), luego el eco por flancos redondeados
(falsa), y solo al **medir** —con un histograma de intervalos escrito para eso—
apareció la causa real. **Medir antes que hipotetizar salió más barato las dos
veces.**

---

## 2026-08-03 — Revisión completa antes de soldar: cuatro correcciones

**Objetivo:** revisar el proyecto entero —política, plan, los 17 ADR, hardware,
protocolo, seguridad, compras y el firmware— buscando errores **antes** de tocar
el cautín, porque a partir de ahí los errores cuestan hardware que no tiene
repuesto.

**Estado del montaje al empezar:** nada soldado, nada conectado.

**Estado del montaje al terminar — todo desconectado a la espera del pedido:**

- **El escritorio no se tocó en ningún momento.** Sigue con su mando original,
  funcionando con normalidad. Nada soldado en la placa del mando.
- **El ESP32 está desconectado** del Mac y de todo lo demás.
- El jumper de prueba entre **P19 y P18 está quitado**.
- **⚠️ El ESP32 tiene cargado `test_capture_ceiling`, NO el sniffer.** Al
  retomar, lo primero es volver a cargar
  [desk_sniffer](../firmware/desk_sniffer/) o no capturará nada. Es el tipo de
  detalle que se pierde entre sesiones y cuesta media hora de desconcierto.
- **Los PC817 están sueltos, comprobados y sin cablear.** Ningún pulsador del
  mando tiene nada soldado — solo el conector JST lleva las tres derivaciones de
  la sonda.
- El escritorio quedó a **80 cm** tras la última medición.

Esta sesión empezó como revisión de documentos —cero mediciones previstas— y
acabó con trabajo real sobre hardware: firmware compilado, cargado y medido. Lo
que sigue distingue una cosa de la otra.

**Plano nuevo:** [hardware/plano_sonda_v2.svg](hardware/plano_sonda_v2.svg), con
el divisor de ADR-016 dibujado. Rehecho una vez porque la primera versión
dibujaba tres símbolos de masa separados y **no se entendía que son un solo
punto** — pregunta directa de quien iba a montarlo, que es la mejor prueba de que
un plano falla. Ahora lleva una barra de masa única con las cuatro conexiones
sobre ella: hilo azul, los dos pies de 27 kΩ y el GND del ESP32.

**Retirado de circulación el plano viejo.** `plano_divisores_v1.svg` no solo
llevaba los valores descartados por ADR-005: dice **"ROJO — no se conecta"** y
manda el divisor al hilo **AMARILLO**. Montarlo habría metido los 5 V por un
divisor a un GPIO y dejado el reloj sin conectar — los dos fallos que la medición
del 2026-08-02 evitó.

Con las resistencias ya en casa y alguien a punto de montar, dos planos de nombre
parecido en la misma carpeta son un riesgo concreto. Se hicieron dos cosas:

1. **Sello `NO MONTAR` superpuesto** al dibujo, que queda intacto debajo.
2. **Renombrado a `plano_divisores_v1_NO_MONTAR.svg`.** Idea de quien iba a
   montarlo, y mejor que el sello: **el nombre se lee antes de abrir el
   archivo; el sello solo después.** Se prefirió el imperativo "no montar" al
   descriptivo "descartado", porque un estado admite lecturas —"descartado como
   preferente, pero quizá sirve"— y una instrucción no.

En el nombre antiguo queda un **archivo redirector**, porque
[ADR-005](DECISIONS.md) enlaza el plano por nombre y los ADR no se editan:
borrarlo dejaría un enlace roto en un documento inmutable.

### Lo que se comprobó y está bien

Antes de las correcciones, lo que sobrevivió a la revisión, porque saber qué
sigue en pie importa tanto como saber qué falló:

- **La aritmética de los divisores es correcta en las cuatro versiones.**
  Recalculados ADR-005, ADR-013, ADR-014 y ADR-016 número por número: todos
  salen. La sonda de ADR-016 —9.1 kΩ / 27 kΩ, bus a 3.99 V, GPIO a 2.99 V— es
  correcta y **no cambia**.
- **El pinout medido es correcto** y coincide con el datasheet.
- **El razonamiento de [ADR-011](DECISIONS.md) es correcto.** Verificado contra
  la tabla de teclado: sin tecla `0x2E` tiene el bit 6 a cero y con tecla a uno,
  así que inyectar exigiría forzar un 1 en un bus open-drain. Imposible, como
  decía.
- **El decodificador de teclas del sniffer coincide con esa tabla**, incluida la
  combinación KI1+KI2 y el enmascarado de los bits *don't care* del comando.
- **El sniffer no puede dañar nada.** Los dos pines son entrada y no se escriben
  en ninguna ruta del código. Ninguno de los pines usados es de arranque, así
  que el ESP32 tampoco los conduce mientras arranca.

### Corrección 1 — el criterio de verificación de la sonda era inaplicable

Cinco documentos repetían: medir rojo↔azul y verde↔azul, esperar ~5 V antes y
~4.0 V después, desconectar por debajo de 3.6 V.

**Un multímetro en continua promedia, y el bus está conmutando.** Lo que se lee
no es el nivel alto de reposo sino el promedio pesado por el ciclo de trabajo,
que no se conoce hasta la primera captura. Las dos lecturas salen por debajo de
lo calculado en una cantidad desconocida, así que el número de después puede
caer por debajo de 3.6 V con la sonda perfectamente bien — y el procedimiento
mandaría desconectar. **Habría parado el proyecto por una falsa alarma.**

El criterio pasa a ser el **cociente** entre las dos lecturas, donde el ciclo de
trabajo se cancela: ≈0.80 esperado, por debajo de 0.70 desconectar. De propina,
el cociente da el pull-up interno real, que era el único parámetro del diseño que
seguía siendo un valor *típico* de datasheet sin mínimo garantizado. El diseño
tolera un pull-up hasta un 70 % más débil que el típico. [ADR-018](DECISIONS.md).

### Corrección 2 — un estado peligroso que no estaba contemplado

**La sonda conectada al bus con el escritorio encendido y el ESP32 sin
alimentar.** El diodo de protección del GPIO engancha el nodo del divisor a
~0.7 V y arrastra el bus a **2.85 V**, por debajo del VIH de 3.5 V: el mando
falla.

No hay daño —son ~210 µA, limitados por los 9.1 kΩ de arriba— pero el síntoma
aparecería justo después de haber soldado en la placa del mando, y la conclusión
natural sería "rompí algo al soldar". Es el mismo patrón que
[ADR-005](DECISIONS.md) evitó: un fallo cuyo síntoma apunta al sitio equivocado.

Regla nueva: USB primero, hilos después; al desmontar, al revés.
[ADR-019](DECISIONS.md).

### Corrección 3 — la impedancia publicada de la sonda estaba mal

ADR-016 daba 6.8 kΩ (= 9.1 ∥ 27). Ese valor solo vale **mientras el chip tira la
línea abajo**. En el flanco de subida la fuente es el pull-up interno, que queda
en serie con la resistencia de arriba: (9.1 + 9.1) ∥ 27 = **10.9 kΩ**. Y el
flanco lento de un bus open-drain es siempre el de subida.

Mismo error en ADR-013: 10.3 kΩ publicados, 13.9 kΩ reales.

**No cambia el diseño ni el orden de preferencia** —ADR-016 sigue siendo mejor
que ADR-013 en esto, 10.9 contra 13.9— pero el margen es menor del que decía el
texto, y el umbral de "por debajo de 200 kHz va sobrado" de
[firmware/README.md](../firmware/README.md) no se sostiene por cálculo. Se
sustituye por un criterio empírico: si `malformed` se queda en cero, sirve.
[ADR-018](DECISIONS.md).

### Corrección 4 — un supuesto escrito como hecho, otra vez

[firmware/README.md](../firmware/README.md) afirmaba "la nuestra es un DevKit de
38 pines con **WROOM-32**". **Nunca se comprobó.** Lo único anotado del
inventario es la serigrafía de la placa de expansión, que no dice nada del
módulo. Y si fuera un WROVER, GPIO16 —el pin elegido para CLK— está ocupado por
la PSRAM.

Es el tercer caso del mismo patrón en este proyecto, después del "ROJO = VCC
probable" del handover y de las resistencias leídas por color.

**Y no se pudo verificar: el blindaje del módulo no es legible.** La única
evidencia disponible es la ficha del vendedor, que dice `ESP-WROOM-32` — el
nombre antiguo de Espressif para el WROOM-32. Apunta a WROOM, pero es un dato de
tienda, exactamente la misma clase de dato que las otras tres veces.

**Así que el supuesto no se resolvió, se eliminó: CLK pasa de P16 a P18**
([ADR-020](DECISIONS.md)). P18 y P4 están libres en WROOM y en WROVER, no son
pines de arranque y no son de la flash. La pregunta deja de tener consecuencias.

Se decidió ahora porque ahora es gratis —no hay nada soldado ni cableado, cuesta
una constante en el sketch— y después costaría desmontar.

Efecto lateral: el ESP32 de repuesto de [COMPRAS.md](COMPRAS.md) ya no tiene que
ser WROOM. Cualquiera de los dos sirve.

### Y con las fotografías del producto, la bornera queda mapeada

Aparecen fotos de la placa de expansión y del DevKit. Resuelven de una vez lo que
quedaba pendiente y dan bastante más:

- **P18 está en la bornera**, columna izquierda, entre P5 y P19. La comprobación
  que ADR-020 dejaba abierta queda cerrada. **P4 está en la misma columna**,
  cuatro posiciones antes.
- **Las dos columnas completas anotadas** en [HARDWARE.md](HARDWARE.md), que
  hasta ahora solo describía tres pines de memoria y con una orientación
  ("bornera derecha con el USB-C hacia abajo") que en las fotos no cuadra. Ahora
  se localizan por vecinos, que no depende de cómo se sujete la placa.
- **Hay un GND dos posiciones más allá de P18**, pasando P19. Más cómodo para el
  retorno de los divisores que el de la esquina, que era el anotado.
- **Peligro nuevo anotado:** la bornera saca `CLK`, `SD0`, `SD1`, `SD2` y `SD3`,
  que son los GPIO 6 a 11 y están cableados a la flash interna. Usarlos impide
  arrancar. Están ahí, con tornillo, junto a los pines buenos.
- **El módulo:** serigrafía `ESP-32`, `FCC ID: 28B77-ESP32-32X`, USB-serie
  **CP2102**. **Verificado que no es un WROVER** — no lo dice por ninguna parte y
  el formato es el corto, con el blindaje ocupando poco más de media placa. Es de
  clase WROOM-32, sin PSRAM. **Supuesto** que sea un WROOM original de Espressif:
  ese marcado y ese FCC ID no son de catálogo Espressif, es un compatible. Da
  igual, que es justo el motivo de ADR-020.
- El CP2102 significa que el puerto será `/dev/cu.usbserial-XXXX` y que macOS no
  necesita driver.

**Pendiente:** las fotos llegaron por la conversación, no como archivos, así que
**no se han podido archivar**. Creada
[hardware/fotografias/](hardware/fotografias/) con el índice de las cuatro que
faltan y las cuatro que hay que tomar, para que se suelten ahí cuando se tengan.

Lo importante es que **los datos ya no dependen de esas fotos**: todo lo que
demostraban está transcrito en [HARDWARE.md](HARDWARE.md) y marcado como
verificado por fotografía. Servirían para volver a comprobarlo sin la placa
delante, no para saberlo.

### La lista de compras estaba escrita para otro país

[COMPRAS.md](COMPRAS.md) daba todo en euros y con proveedores implícitos
europeos. **El proyecto se compra en Bogotá.** Una lista que no se puede ejecutar
donde está la persona es una lista mala, por muy bien razonada que esté.

Corregido: precios convertidos a pesos como orden de magnitud —a ~3.600 COP/€,
tasa de agosto de 2026— **marcados explícitamente como referencia y no como
precio local**, porque lo importado en Colombia sale más caro y presentar una
conversión como si fuera un precio de tienda es otro supuesto disfrazado de dato.
Añadida una tabla de tiendas colombianas (Sigma Electrónica, Vistrónica,
Didácticas Electrónicas, Electronilab, DynamoElectronics, MercadoLibre), con la
advertencia de confirmar dirección y horario, que no están verificados en sitio.

Anotado también lo único difícil de conseguir localmente: los **photoMOS** de
fase 3. Si no aparecen en tienda, hay que pedirlos fuera y contar con la espera.
No bloquean nada ahora.

### Candidato local a los photoMOS: PC817

Como los photoMOS son lo difícil de conseguir en Bogotá, se evaluaron dos módulos
optoacopladores que sí hay en tienda. **Evaluación sobre fotografías y fichas de
vendedor, no sobre el mando real.**

- **Módulo TLP281-4 (rojo): descartado.** Lleva seis transistores S8050
  (marcados `J3Y`) en la salida y conectores con VCC y GND a ambos lados. Es un
  aislador de señales lógicas, no un sustituto de un botón. Un botón tiene dos
  patas libres; esto tiene una salida referida a masa.
- **Módulo PC817 de 2 canales (verde, con borneras): candidato válido, con una
  condición.** Su jumper amarillo selecciona salida con *pull-up* o con
  *pull-down* —lo dice la ficha y lo corroboran los jumpers visibles en la
  fotografía—. Cualquiera de las dos inyectaría corriente permanente en la matriz
  del teclado. **El jumper se quita**, para dejar el fototransistor desnudo entre
  los dos bornes.

Los números del PC817 salen con margen grande contra los 30–90 µA que hay que
conmutar: fuga apagado ≤ 0.1 µA (0.1–0.3 % del pull-down, no finge una tecla) y
capacidad de paso de 0.7–2 mA con solo 0.7 mA de LED. **La carga es tan pequeña
que hasta da igual que el GPIO dé 3.3 V y la ficha pida 3.6 V mínimo.**

Sobre la ficha del vendedor, que es en su mayor parte texto de marketing
generado: los números que sí son verificables —80 V de V_CEO, 50 mA de I_C,
5000 V de aislamiento— coinciden con el datasheet del PC817. El dato que importa,
el del jumper, está corroborado por la fotografía. El resto no se usó.

**Queda sin verificar lo que decide**: que la corriente por el botón vaya siempre
en el mismo sentido. Un PC817 conduce en una sola dirección; un botón y un
photoMOS, en las dos. Lo responde la captura del bus. **Es un candidato anotado
en [COMPRAS.md](COMPRAS.md) con sus dos pruebas de multímetro pendientes, no una
decisión**: [ADR-017](DECISIONS.md) sigue vigente.

**Un hallazgo extra al revisar los pines:** el ejemplo de cabecera de
[capturas/README.md](capturas/README.md) —una plantilla pensada para copiarse—
decía `sonda 10k/20k en P16 (amarillo)`. Las dos cosas mal: esa sonda nunca se
adoptó, y el **amarillo es el hilo de 5 V**, que no se conecta jamás. Corregido y
anotado allí mismo.

### Dos bugs más en el sniffer

Los dos son herederos del desbordamiento del contador de ciclos de 32 bits que
la revisión del 2026-08-02 creyó cerrado, y los dos corrompen datos de captura en
silencio:

1. **El periodo de reloj mínimo se podía envenenar.** El comentario del código
   decía que la cuenta se reinicia por transacción; el código no lo hacía. El
   primer flanco de cada trama medía hacia atrás hasta la trama anterior,
   cruzando el hueco muerto, y si ese hueco pasaba de 17.9 s —el chip dormido,
   [ADR-012](DECISIONS.md)— la resta daba la vuelta y dejaba un mínimo falso y
   minúsculo. Justo el número que decide si el divisor sirve.
2. **La marca de tiempo podía saltar 17.9 s.** Si la interrupción encolaba un
   evento justo después de que el bucle diera la cola por vacía, el *keep-alive*
   adelantaba la base por delante de ese evento y la resta sin signo lo convertía
   en un salto enorme, permanente a partir de ahí.

Ambos corregidos.

**Y por primera vez el sniffer ha pasado por un compilador.** No por la toolchain
del ESP32, que sigue sin instalarse, sino por clang en el Mac con `Arduino.h` y
`soc/gpio_struct.h` reducidas a mano a lo que el sketch usa, con
`-fsyntax-only -Wall -Wextra`: **cero errores y cero avisos**, en las dos ramas
de `cycleCount()`.

Qué descarta eso y qué no, porque la diferencia importa: descarta erratas,
nombres sin declarar y errores de tipo, que es la mayor parte de lo que falla al
compilar algo escrito a mano. **No descarta** desajustes con las firmas reales
del core de Arduino, porque las cabeceras eran aproximaciones mías, ni nada del
comportamiento en ejecución. **Sigue sin ejecutarse nunca contra hardware.**

Un detalle, para que no se repita el susto: con `__XTENSA__` definido, clang
rechaza la línea de ensamblador `rsr %0, ccount` por su restricción `"=a"`. **Es
un falso positivo**, no un fallo del firmware: `"=a"` es la restricción correcta
de registro en Xtensa y clang la rechaza porque estaba compilando para arm64. No
se tocó.

### Precisiones menores

- [HARDWARE.md](HARDWARE.md) decía que el material entre 1 kΩ y 10 kΩ suma
  43 kΩ. En esa banda suman **41.7 kΩ**; los 43 salían de incluir las de 800 Ω y
  555 Ω, que no sirven para esto. La conclusión —no alcanza— no cambia.
- [COMPRAS.md](COMPRAS.md) decía "conectar el analizador lógico a través de los
  mismos divisores", ambiguo. Montarle un **segundo** divisor de los mismos
  valores carga el bus con 18 kΩ y lo hunde a 3.32 V, por debajo del umbral.
  Aclarado: el mismo nodo, en paralelo.
- Los 470 Ω para el LED de los photoMOS dan 4.4 mA, en el límite de lo que estos
  componentes piden. Pasan a **330 Ω**, 6.2 mA, a confirmar contra el datasheet
  de la pieza que se compre.
- El plan B de la sonda (buffer 74LVC2G17) quedó huérfano cuando ADR-016
  reemplazó a ADR-013. Re-anclado en [ADR-018](DECISIONS.md).

### Hueco de seguridad anotado, sin decidir

[ADR-010](DECISIONS.md) exige temporizador independiente por hardware **solo para
M1 y M2**, porque el fallo que tenía en mente era sobrescribir un preset. Pero un
contacto pegado en **subir** mueve el escritorio hasta el tope, y
[ADR-009](DECISIONS.md) depende de mantener ese canal cerrado durante segundos.
Las protecciones para ese caso viven todas en el firmware, que es lo que se
supone que ha fallado. Anotado en [SEGURIDAD.md](SEGURIDAD.md) como bloqueante
de la fase 3. **No se decide aquí**: la fase 3 no está abierta.

### El atajo del puerto de accesorios queda descartado

**Comprobado por inspección visual: la caja de control no tiene puerto de
accesorios.** Solo la entrada del adaptador de 29 V, el conector de 4 hilos del
mando y el cable de 6 hilos de los motores. Ningún RJ11, RJ12 ni RJ45.

Era la única vía que podía acortar el proyecto entero: esas cajas Jiecang con
puerto serie 9600 8N1 tienen componentes de ESPHome ya escritos, y habrían hecho
innecesarios el sniffer, el accionamiento y todo lo que hay que soldar en el
mando. Cierra el paso C de [PLAN.md](PLAN.md) en negativo.

Era lo esperable —un mando con AiP650E indica caja de gama sencilla— y el
resultado sirve igual: **deja de haber una alternativa pendiente de explorar.**
El bus del mando es el único camino, y ahora se sabe en vez de suponerse.

Corregido de paso un supuesto de [HARDWARE.md](HARDWARE.md), que daba por hecho
que la caja tenía "al menos tres conectores: uno por motor y uno para el mando".
Los conectores reales están ahora anotados como verificados.

### El sniffer compila, carga y arranca ✅

**Instalado Arduino IDE con el core de Espressif y cargado el sniffer en el
DevKit real.** Es la primera vez que este firmware pasa por un compilador de
verdad; hasta hoy solo tenía un análisis sintáctico con cabeceras simuladas.

**Qué se esperaba antes de hacerlo:** que compilara con algún error menor a la
primera —lo normal en código escrito sin toolchain— y que la autocomprobación
reportara cero flancos por no haber nada conectado.

**Qué pasó:** compiló a la primera, sin errores. Arrancó, el serie a 921600
funciona y la autocomprobación reportó exactamente lo previsto:

```
Identifying lines (2 s)...
  GPIO18 (expected CLK, red)  : 0 edges
  GPIO4 (expected DIO, green): 0 edges
  !! No activity on either line.
```

**Estado del montaje:** ESP32 alimentado solo por USB desde el Mac. **Nada
conectado al escritorio.** El escritorio ni se tocó.

**Qué queda demostrado:** que compila con la toolchain real, que arranca, que el
puerto serie a 921600 va, que las dos interrupciones se enganchan y que
`checkLines()` funciona. Cae el último desconocido del software antes de tocar
hardware.

### Prueba de captura con ruido — la cadena entera funciona

Con el ESP32 solo por USB y **nada conectado al escritorio**, se inyectó ruido en
GPIO18 tocando el pin (entrada flotante de alta impedancia captando el zumbido de
la red a través del cuerpo). Contadores reiniciados antes con `c`:

```
  edges captured : 197548
  edges dropped  : 0
  transactions   : 0 (0 malformed, 0 ended by repeated START)
```

**Tres resultados, y los dos últimos valen más que el primero:**

1. **La cadena de captura funciona de punta a punta.** El pin lee, la
   interrupción dispara, la cola encola y el contador cuenta.
2. **Cero flancos perdidos en ~200.000.** Descarta un problema estructural en la
   ruta de interrupción. **No mide la capacidad máxima** — ver abajo.
3. **Cero transacciones inventadas.** 200.000 flancos de basura aleatoria y el
   decodificador **no fabricó ni una sola transacción ni una malformada**. Valida
   la detección de START y STOP: no alucina protocolo donde no lo hay. Es el
   resultado negativo que más tranquilidad da, porque el modo de fallo temido en
   la fase 2 es justo el contrario — creerse datos que no existen.

### Segunda pasada cronometrada — y una corrección

Se repitió la prueba midiendo el tiempo: **14.951 flancos en 5 segundos ≈ 3.000
flancos/s**, cero perdidos.

Trece veces menos que la primera pasada. **Ese contraste es el hallazgo**: la
tasa depende de cómo acople el dedo, no de lo que el ESP32 aguanta. Es decir,
**este método no puede medir el techo de captura**, solo confirmar que hay
camino.

*Corrección de lo que se dijo en esta misma sesión antes de cronometrar: se
estimó que la primera pasada rondaba los 40.000 flancos/s "justo el orden de
magnitud necesario". Esa estimación suponía 5 segundos de duración, que nunca se
midieron. No se sostiene.*

**Lo que queda establecido, con precisión:** un **suelo** de ~3.000 flancos/s
sostenidos sin pérdidas, y ningún indicio de problema estructural. **El techo
sigue siendo desconocido.**

**Estimación de lo que hará falta, sin verificar:** una transacción son 16 bits
más ACK, o sea 18 periodos de reloj ≈ 36 flancos de CLK más hasta 18 de DIO,
unos 54 flancos. Si la caja refresca los cuatro dígitos cada ~8 ms con varias
transacciones, salen del orden de **decenas de miles de flancos por segundo**.
Eso está por encima del suelo medido, así que **la pregunta sigue abierta**.

**Cómo se cerró:** ver abajo. Se hizo.

### Medida del techo de captura — el ESP32 midiéndose a sí mismo

Escrito [firmware/test_capture_ceiling/](../firmware/test_capture_ceiling/): un
sketch que genera una onda cuadrada de frecuencia conocida por GPIO19, la mete
por GPIO18 con un jumper y sube la frecuencia contando lo que se pierde. El ISR
y la cola están copiados literalmente del sniffer para que el número signifique
algo. Es la respuesta del proyecto a no tener analizador lógico: **el ESP32 hace
de instrumento de sí mismo.**

**Qué se esperaba:** que empezara a perder flancos en algún punto entre 50 y
200 kHz, y no tener ni idea de dónde exactamente.

**Resultado, con cero flancos perdidos en todos los escalones:**

| Onda cuadrada | Flancos/s esperados | Capturados | Perdidos |
|---|---|---|---|
| 1 kHz | 2.000 | 2.000 | 0 |
| 10 kHz | 20.000 | 19.997 | 0 |
| 50 kHz | 100.000 | 99.981 | 0 |
| **100 kHz** | **200.000** | **199.961** | **0** |

El déficit de decenas de flancos sobre cientos de miles **no son pérdidas**
—`dropped` es 0 en todos— sino los que caen fuera de la ventana de un segundo.

**Conclusión, que es la que importa:** dentro de una transacción los flancos
llegan al doble de la frecuencia del reloj del bus. **Si el bus del escritorio
corre a 100 kHz o menos, la captura no pierde nada.** Esta familia de chips suele
correr bastante por debajo. La sonda resistiva y su redondeo de flancos siguen
siendo la incógnita, pero **la ruta de interrupción deja de serlo.**

### Hallazgo mayor: el contador `dropped` tiene un punto ciego

La segunda pasada, con el sketch corregido, dio números **exactos** hasta
125 kHz — 250.000 de 250.000 flancos, sin margen de error. Y luego esto:

| Onda cuadrada | Esperados | Capturados | Faltan | `dropped` |
|---|---|---|---|---|
| 100 kHz | 200.000 | 200.000 | 0 | 0 |
| 125 kHz | 250.000 | 250.000 | 0 | 0 |
| **150 kHz** | 300.000 | **299.613** | **387** | **0** |

**Faltan 387 flancos y el contador de descartes marca cero.** No pueden ser
efectos de borde de ventana: los dos escalones anteriores salieron clavados.

**La explicación:** a 300.000 flancos/s hay 3,3 µs entre uno y otro, y a veces
llegan dos tan pegados que el ISR aún está ejecutando el primero. El registro de
estado del GPIO anota *que hubo* interrupción, no *cuántas*: los dos flancos
colapsan en una sola llamada al ISR y el segundo **desaparece sin dejar rastro**.

**Por qué importa más allá de esta prueba:** el sniffer usa `dropped` como señal
de que el bus supera su capacidad. Ese contador solo ve los flancos que llegaron
al ISR y no cupieron en la cola. **Los que nunca dispararon la interrupción no
incrementan nada.** Es decir, el sniffer puede estar perdiendo datos y reportar
salud perfecta.

**Consecuencia para la fase 2 — cuál es el canario de verdad:** no es `dropped`,
es **`malformed`**. Un flanco perdido en mitad de una transacción descuadra la
cuenta de bits y la transacción sale marcada como malformada. Ese contador sí
detecta esta pérdida silenciosa. **La regla operativa pasa a ser: `dropped` en
cero no basta; lo que hay que vigilar es `malformed` en cero.**

*Corrección de lo escrito antes en esta misma entrada: se dijo que "el contador
`dropped` existe para avisar de que el bus supera la capacidad de captura". Es
cierto pero incompleto, y la parte que falta es justo la peligrosa.*

### Tercer hallazgo: por encima del techo el ESP32 no degrada, se muere

El barrido volvió a colgarse en el escalón siguiente, **175 kHz** (350.000
flancos/s), ya con la ventana cronometrada por contador de ciclos. No imprimió
nada más: ni la línea del escalón, ni el veredicto final.

**Mecanismo exacto: sin determinar.** Puede ser el watchdog de interrupciones
entrando en pánico, o saturación total del CPU por otra vía. La corrección del
`millis()` era correcta pero atacaba solo una de las formas de morir. **No se
investigó más porque no cambia ninguna decisión del proyecto**, y averiguarlo
costaría más que el valor que aporta.

**Lo que sí queda establecido, y es lo importante:**

| Régimen | Comportamiento |
|---|---|
| ≤ 125 kHz | Captura exacta, sin pérdidas |
| 150 kHz | **Pierde flancos en silencio**, sin incrementar ningún contador |
| ≥ 175 kHz | **El ESP32 deja de responder** |

**No hay degradación suave.** Se pasa de perfecto a silenciosamente incorrecto a
muerto, en menos de un factor de dos de frecuencia.

**Consecuencia para la fase 3, que era precaución teórica y ahora es evidencia:**
[ADR-010](DECISIONS.md) exige que los pulsos de relé se acoten con un
**temporizador independiente del firmware**, capaz de abrir el relé aunque el
programa esté colgado. Hasta hoy eso se justificaba con "un cuelgue del
firmware", en abstracto.

Ya no es abstracto: **acabamos de colgar este ESP32 dos veces seguidas, con una
señal externa, sin ningún bug de lógica.** Una condición eléctrica en un pin
basta para dejarlo sin responder. Si eso pasa con un relé de "subir" cerrado, lo
único que para el escritorio es hardware que no dependa del CPU.

Refuerza también el punto pendiente anotado en [SEGURIDAD.md](SEGURIDAD.md) sobre
acotar por hardware **también subir y bajar**, no solo M1 y M2.

### Cuarto hallazgo, y el más incómodo: el sketch aprobaba con el cable quitado

Se repitió el barrido **desconectando el jumper de P19-P18**, para ver qué decía.
Dijo esto:

```
    1000 Hz | expected      2000 | captured      1764 ( 88%) | dropped  0 OK
   50000 Hz | expected    100000 | captured         0 (  0%) | dropped  0 OK
  400000 Hz | expected    800000 | captured         0 (  0%) | dropped  0 OK

RESULT: clean up to 400000 Hz  (800000 edges/s) with zero drops.
```

**Cero flancos capturados, y el veredicto es un aprobado con nota.** Sin cable.

**La causa, que es un error de diseño mío:** el criterio de éxito era
`dropped == 0`. Y ese contador vale cero cuando **no llega ningún flanco al
ISR**. Nada conectado ⇒ nada que descartar ⇒ "perfecto" en todos los escalones.

**Lo grave no es el bug, es que es un bug repetido.** La revisión del 2026-08-02
ya había encontrado exactamente esta clase de fallo en `checkLines()` del propio
sniffer —"la comprobación de líneas daba OK con un cable cortado"— y quedó
anotado en esta misma bitácora. Se volvió a cometer en un archivo nuevo dos
semanas después.

**Corregido.** El criterio pasa a ser *llegaron los flancos que tenían que
llegar*: `capturados + holgura >= esperados`, con holgura de 4 flancos o el 0.1%,
lo que sea mayor. Además distingue tres fracasos distintos —sin señal, cola
llena, y pérdida silenciosa— en vez de un OK indiscriminado.

**Comprobado contra las tres corridas reales antes de darlo por bueno:**

| Corrida | Criterio viejo | Criterio nuevo |
|---|---|---|
| Sin jumper, 1 kHz | OK ❌ | **FALLA**, "is the jumper on?" ✅ |
| Con jumper, ≤125 kHz | OK ✅ | OK ✅ |
| Con jumper, 150 kHz | OK ❌ | **FALLA**, "losing edges silently" ✅ |

El criterio viejo daba por buenos los dos casos que importaban. El nuevo los
caza.

**Lección, y es la misma de siempre en este proyecto:** una comprobación que solo
mira contadores de error, sin verificar que hubo señal, **no comprueba nada**. Un
cero puede significar "todo bien" o "no hay nadie ahí", y hay que distinguirlos
explícitamente. Vale para `dropped`, vale para `malformed`, y valdrá para
cualquier diagnóstico de la fase 2: **antes de creerse un contador en cero, hay
que probar que había algo que contar.**

### El sketch de prueba colgó la placa, y era un fallo de diseño mío

A 200 kHz el sketch dejó de responder. **No fue un cuelgue del ESP32 sino una
tormenta de interrupciones**, y la causa era el propio sketch:

- A 200 kHz llegan **400.000 flancos/s**. El CPU se satura entrando y saliendo
  del ISR.
- La ventana de medida se cronometraba con `millis()`. Bajo saturación, el tick
  del temporizador **también se queda sin CPU**, `millis()` deja de avanzar y la
  condición de salida no se cumple nunca.

Corregido: la ventana se cronometra ahora con el **contador de ciclos del CPU**,
que es una lectura de registro y no la puede starvar nada. Además la interrupción
solo se engancha durante la medición —no mientras se imprime o se cambia de
frecuencia— y el barrido **se detiene en el primer escalón que pierda algo**,
porque más allá del techo no hay nada que aprender.

**Lección que vale más que el sketch:** cualquier espera cronometrada con
`millis()` deja de funcionar bajo carga de interrupción alta. El sniffer no tiene
ese patrón en su ruta crítica —revisado— pero es una trampa a recordar en fase 3,
donde va a haber temporizadores acotando pulsos de relé.

**Nota sobre el comando `l`:** su ventana de comprobación son 2 segundos y hay que
estar ya generando flancos al pulsarlo, cosa que no es evidente. La vía cómoda es
`c`, generar actividad sin prisa, y `s`. Conviene recordarlo al depurar en frío.

**Qué sigue sin demostrarse:** el decodificador contra datos reales. Necesita bus.

### Siguiente paso

Sin cambios de fondo: **comprar dos resistencias de 27 kΩ**. Se puede soldar
mientras tanto. Orden completo en [PLAN.md](PLAN.md).

---

## 2026-08-02 — Identificación de los 4 hilos y lectura del datasheet

**Objetivo:** saber qué es cada hilo del conector y cuánta carga admite el bus,
para poder dimensionar la sonda y empezar a soldar.

**Estado del montaje al empezar:** nada conectado al ESP32. Escritorio
**desenchufado de la corriente**.

### Qué se esperaba

Que el azul cayera en la pata 15 (GND) y el rojo en la 16 (VDD), con amarillo y
verde en las patas 5 y 6. Pull-up entre 1 kΩ y 10 kΩ.

**No pasó nada de eso, y menos mal.**

---

### Medición 1 — continuidad hilo ↔ pata del chip

Conector desenchufado de la caja de control. Numeración: pata 1 en la esquina
del punto hundido, pata 16 la de enfrente.

| Hilo | Pata | Ohmios |
|---|---|---|
| Rojo | 2 | 0.2 Ω |
| Verde | 3 | 0.2 Ω |
| Azul | 4 | 0.2 Ω |
| Amarillo | 10 | 0.2 Ω |

Contra las patas 15 y 16, ningún hilo marca nada.

El resultado se descartó dos veces como error antes de aceptarlo: primero se
sospechó de caminos falsos a través de la caja de control (se repitió con el
conector suelto, mismo resultado), después de un umbral de continuidad
permisivo (se repitió en ohmios, 0.2 Ω, que es resistencia de pista).

**La medición era correcta desde el primer intento. Lo que estaba mal era el
pinout de referencia**, tomado de components101: daba 5 = SCL, 6 = SDA,
15 = GND, 16 = VDD. El pinout real del AiP650E es **2 = CLK, 3 = DIO, 4 = GND,
10 = VDD**, y encaja pata por pata con lo medido.

**Pinout del cable verificado:**

| Hilo | Función |
|---|---|
| Rojo | CLK — reloj del bus |
| Verde | DIO — datos del bus |
| Azul | GND |
| Amarillo | VDD, 5 V |

### Corrección de un supuesto heredado

El handover daba el **rojo** como VCC probable y el **amarillo** como línea de
datos. Es al revés en los dos.

De haberse montado el plan original, el **reloj habría quedado sin conectar**
—captura vacía— y la **línea de 5 V habría ido a un GPIO** por un divisor. El
síntoma, "no se ve tráfico", no señalaba a ninguno de los dos errores.

Se evitó porque el handover marcaba ese punto como *supuesto* y no como hecho, y
porque no se soldó nada hasta verificarlo. Es exactamente para lo que existe la
regla 5 de la [política](POLITICA_DOCUMENTACION.md).

Corregido en [HARDWARE.md](HARDWARE.md), [PROTOCOLO.md](PROTOCOLO.md) y
[REFERENCIAS.md](REFERENCIAS.md). El pinout equivocado queda anotado como tal en
los tres sitios, no borrado.

---

### Lectura del datasheet oficial — el proyecto queda desbloqueado

Conseguido el PDF completo de I-CORE (*AiP650E Product Specification*,
`AiP650E-AX-XS-B037EN`, 2024-01-B1, 15 páginas) y leído entero. Confirma el
pinout medido y trae mucho más.

**Hallazgo principal: el pull-up del bus es interno al chip.** Página 5, CLK
tiene *built-in pull-up resistors* y DIO es *N-Channel, Open-Drain*; página 6
los cuantifica en **550 µA típicos**, unos **9.1 kΩ** a 5 V.

Con eso **la medición del pull-up deja de ser necesaria** — era lo único que
bloqueaba el montaje. Sonda definida en [ADR-013](DECISIONS.md): divisor de
15 kΩ / 33 kΩ, que deja el bus a 4.20 V (mínimo exigido 3.5 V) y entrega 2.89 V
al GPIO. Hay que comprar 2×15 kΩ y 2×33 kΩ; no hay nada aprovechable en el
inventario.

**Confirmaciones contra el chip real**, que hasta ahora eran préstamos del
TM1650 y ya no lo son:

- Comandos `0x48` sistema, `0x68`/`0x6A`/`0x6C`/`0x6E` dígitos, `0x49` teclado.
- Mapa de segmentos A = bit 0 … DP = bit 7.
- Tabla completa de códigos de teclado. **Sin tecla = `0x2E`; con tecla, bit 6 a
  uno.** Confirma [ADR-011](DECISIONS.md) desde la fuente primaria: inyectar
  exigiría llevar un bit de 0 a 1 en un bus open-drain.

**Datos nuevos:**

- **Duración mínima de pulsación: ~160 ms.** El chip solo reconoce una tecla si
  dura dos periodos de escaneo, y el periodo llega a 80 ms. Es el suelo que le
  faltaba a [ADR-010](DECISIONS.md), que hasta ahora solo tenía techo.
- **El modo sueño existe explícitamente** (bit 2 del byte de display).
  [ADR-012](DECISIONS.md) deja de ser precaución teórica.
- **El chip admite combinaciones KI1+KI2** sobre el mismo DIG, con prioridad
  máxima. Si alguna función del mando usa combinación, un relé solo no la
  reproduce.
- El comando de teclado es `0100_1XX1`, con dos bits *don't care*: `0x49`, `0x4B`,
  `0x4D` y `0x4F` son el mismo comando. El decodificador debe enmascarar, no
  comparar contra `0x49`.
- Toda transacción es de **16 bits**: byte de comando o dirección + ACK, byte de
  dato + ACK. Regla útil para detectar flancos perdidos.
- I-CORE llama a las líneas **CLK y DIO**, no SCL/SDA. Alimentación 3–5.5 V.
  Display de cátodo común.
- El circuito de aplicación recomendado lleva 220 Ω en serie entre el conector y
  CLK/DIO. **Esta placa no los lleva** — se deduce de los 0.2 Ω medidos.

### Interpretación

- **Verificado:** pinout del cable y del chip, por medición propia coincidente
  con el datasheet del fabricante.
- **Verificado por datasheet:** pull-ups internos, comandos, segmentos, códigos
  de teclado, tiempos de escaneo, existencia del modo sueño.
- **Pendiente de captura:** cómo usa la caja de control todo esto. Las siete
  preguntas abiertas están en [PROTOCOLO.md](PROTOCOLO.md).
- **Duda menor sin resolver:** al medir ~5 V contra el azul en su día, ¿el
  multímetro mostraba signo negativo? Ya no importa — la continuidad al chip
  resolvió la identificación de los hilos por otra vía.

### Estado del montaje al terminar

Sin cambios físicos. Nada soldado, nada conectado. Mando y escritorio intactos y
funcionales. El conector de 4 pines se desenchufó y se volvió a enchufar.

### Siguiente paso

**Comprar dos resistencias de 27 kΩ** — con las 9.1 kΩ ya medidas, es todo lo
que falta ([ADR-016](DECISIONS.md)).

Se puede soldar mientras tanto, que no depende de ellas: tres hilos al conector
del mando (rojo, verde y azul; el amarillo no). Y de paso, mirar la caja de
control por fuera a ver si tiene un puerto serie adicional (paso C de
[PLAN.md](PLAN.md)), que sería un atajo grande.



Pendiente menor: copiar las fotografías macro del chip y de la serigrafía a
`hardware/`. El datasheet ya está archivado en `hardware/datasheets/`.

### Firmware del sniffer, mismo día

Escrito [firmware/desk_sniffer/](../firmware/desk_sniffer/) contra el datasheet,
sin hardware conectado. Decodifica altura, teclas y control de display.

Arquitectura: la ISR solo pone marca de tiempo, lee los dos pines y encola; todo
el decodificado ocurre en `loop()`. Así la ISR es corta y, si el bus fuera más
rápido de lo que el ESP32 aguanta, se nota como descartes contados en vez de
como datos corruptos silenciosos.

Comprobaciones que trae incorporadas, cada una por un motivo concreto:

- **Identificación de líneas al arrancar.** Cuenta flancos 2 s en cada pin. CLK
  lleva ráfagas por transacción y DIO cambia como mucho una vez por bit, así que
  si DIO tiene más flancos, los cables están cruzados. Es el error de montaje
  más probable y produce basura que parece problema de protocolo.
- **Contador de transacciones malformadas.** El datasheet dice que todas son de
  16 bits; cualquier otra cosa es un flanco perdido.
- **Contador de descartes de buffer** y **medida del periodo de reloj más
  corto**, que es lo que dirá si el divisor resistivo aguanta o hace falta el
  buffer del que habla [ADR-013](DECISIONS.md).

**Sin compilar.** No hay toolchain de Arduino en el Mac. El código está escrito
y revisado a mano, pero nunca pasó por un compilador ni por hardware.

### Revisión adversarial del sniffer, mismo día

Revisión buscando fallos, no confirmaciones. Ocho hallazgos, todos corregidos.

**Serios:**

1. **Desbordamiento del contador de ciclos.** Los tiempos se calculaban con el
   contador de ciclos de la CPU, de 32 bits, que a 240 MHz da la vuelta cada
   17.9 s. Si el bus se queda callado más que eso, la marca de tiempo se va al
   garete en silencio. Y es justo el escenario de
   [ADR-012](DECISIONS.md): el chip puede dormirse. Corregido con un
   *keep-alive* que absorbe ciclos mientras el bus está callado.
2. **La comprobación de líneas daba "OK" con un cable cortado.** Solo comparaba
   cuál de los dos tenía más flancos; con DIO a cero seguía diciendo que el
   cableado era consistente, y después todos los bytes saldrían `0x00`.
   Ahora avisa por separado de las dos líneas muertas.

**Menores, pero reales:**

3. Un START repetido —sin STOP en medio— descartaba la transacción anterior sin
   dejar rastro. Ahora se emite y se cuenta aparte.
4. El periodo de reloj mínimo se medía en microsegundos enteros, que a
   velocidades altas trunca a cero o miente. Se mide en ciclos y solo dentro de
   una transacción, para que el hueco entre transacciones no lo falsee.
5. **El contador de flancos descartados podía dispararse por culpa nuestra**, no
   del bus: con el volcado crudo encendido, el puerto serie se satura y bloquea
   el bucle mientras la interrupción sigue encolando. Mitigado con más buffer de
   transmisión, y el propio mensaje de estadísticas ahora avisa de que hay que
   repetir con el volcado apagado antes de sacar conclusiones.
6. La línea de tecla pulsada se imprimía en cada sondeo, unas 25 veces por
   segundo mientras se mantiene un botón. Ahora solo en los cambios, que además
   es lo que sirve para correlacionar botones.
7. Variable muerta y asignación encadenada de variables `volatile`.
8. División por cero posible si `getCpuFrequencyMhz()` devolviera 0.

Sigue sin compilar y sin probar contra hardware. Una revisión a mano encuentra
errores de razonamiento; no encuentra errores de compilación ni sorpresas del
bus real.

### Inventario de resistencias y desbloqueo del montaje, mismo día

Leído el código de colores de todo el surtido y confirmado con multímetro que
las dos de blanco-café-rojo son **9.1 kΩ**, no los 920 Ω que se habían anotado
en su día. Inventario completo en [HARDWARE.md](HARDWARE.md).

Primer cálculo: no alcanzaba. Dos divisores necesitan unos 90 kΩ repartidos en
cuatro piezas de valor medio, y todo el surtido por encima de 1 kΩ suma unos
64 kΩ. Con 15 kΩ arriba solo salía un divisor, y con seis resistencias en serie.

Rehecho el reparto poniendo **9.1 kΩ arriba** en vez de 15 kΩ, sí sale, y con
una ventaja inesperada: la impedancia que ve el GPIO baja de 10.3 kΩ a 6.5 kΩ,
así que los flancos se redondean menos que en el diseño preferente. Registrado
como [ADR-014](DECISIONS.md), con verificación obligatoria del nivel del bus al
conectar, porque el margen sobre el umbral del chip baja de 0.7 V a 0.38 V.

Descartada por poco margen la combinación 9.1 k / 15 k: deja el bus a 3.63 V con
el umbral en 3.5 V, sobre un pull-up que el datasheet solo da como *típico*.

### Y se vuelve a bloquear, media hora después

Las dos resistencias que sostenían ADR-014 **no son de 15 kΩ, son de 75 kΩ**: la
banda leída como marrón era violeta. Con 75 kΩ no hay recambio — emparejadas con
las 9.1 kΩ dan 4.0 V en el GPIO, por encima de lo que tolera el ESP32, y
corregir el reparto exige más material del que hay. **Ninguna combinación del
inventario real monta los dos divisores.**

ADR-014 queda **anulado por [ADR-015](DECISIONS.md)** sin haberse montado nunca.

Tercera corrección de inventario del día, y la segunda que cambia el diseño:

| Se leyó | Es | Consecuencia |
|---|---|---|
| 920 Ω | 9.1 kΩ | Desbloqueó ADR-014 |
| naranja-azul-café, 360 Ω | naranja-azul-negro, 36 Ω | Ninguna |
| marrón-verde-naranja, 15 kΩ | violeta-verde-naranja, 75 kΩ | Anuló ADR-014 |

La causa no es descuido: **el código de colores no tiene redundancia.** Un color
mal leído mueve el valor un factor de mil, y marrón contra violeta en una banda
pequeña es una confusión ordinaria. De ahí la segunda mitad de ADR-015: los
valores se miden con el multímetro, que ya se tiene, y el código de colores solo
sirve para localizar una pieza en el cajón.

### Inventario medido pieza a pieza — sonda final

Medidas las 30 resistencias con multímetro. Resuelve la confusión: las dos que
parecían de 15 kΩ y luego de 75 kΩ son **7.4 kΩ** (violeta-verde-**rojo**); de
74 kΩ solo hay una.

Aparecen **dos de 9.1 kΩ** confirmadas, que son un valor excelente para la parte
de arriba del divisor. Con eso la compra necesaria baja de cuatro resistencias a
**dos**, y la sonda final queda fijada en [ADR-016](DECISIONS.md): **9.1 kΩ
arriba, 27 kΩ abajo**, con el bus en 3.99 V y el GPIO en 2.99 V — las dos
tensiones centradas en su rango. Gana además en impedancia sobre ADR-013:
6.8 kΩ en vez de 10.3 kΩ, así que redondea menos los flancos.

Sigue sin poder montarse solo con el inventario, y ahora se sabe por cuánto:
cada divisor necesita ~25 kΩ, dos necesitan 50 kΩ, y todo el material entre
1 kΩ y 10 kΩ suma **43 kΩ**. Faltan siete. La de 74 kΩ permitiría una línea
buena pero consume todo el material medio y deja la segunda en 2.2 V.

Inventario completo en [HARDWARE.md](HARDWARE.md), ya sobre valores medidos y no
leídos.

Lección de proceso, que es lo que hay que llevarse: un supuesto disfrazado de
dato —un valor "leído" anotado en una tabla junto a valores medidos— se comporta
igual que el "ROJO = VCC probable" del handover. La tabla ahora distingue
explícitamente lo medido de lo leído.

### Sonda final y lista de compras

Inventario medido pieza a pieza, 30 resistencias. Sonda fijada en
[ADR-016](DECISIONS.md): **9.1 kΩ arriba, 27 kΩ abajo**, bus a 3.99 V y GPIO a
2.99 V. Falta comprar solo **dos resistencias de 27 kΩ**.

Hallazgo aparte, al preparar el pedido: **el módulo de relés mecánicos del
inventario no sirve para el accionamiento.** Los pulsadores conmutan
microamperios —2 kΩ en serie en la matriz y pull-down interno de 50 µA— y un
contacto mecánico por debajo de su corriente mínima se vuelve intermitente con
el tiempo, con un modo de fallo que aparece meses después. Se sustituye por
relés de estado sólido optoacoplados (photoMOS). [ADR-017](DECISIONS.md).

Lista de compras razonada en [COMPRAS.md](COMPRAS.md), archivo nuevo. Lo de
mayor valor por euro que hay ahí: un **analizador lógico USB de 10 €**, que
elimina la restricción de partida del proyecto —"no hay instrumentos, el ESP32
tiene que hacer de instrumento"— y convierte la fase de captura de "a ciegas" en
"mirando".

### Archivos generados

ADR-011 a ADR-017. Firmware del sniffer, en
[firmware/desk_sniffer/](../firmware/desk_sniffer/). Inventario completo de
resistencias, medido pieza a pieza. [COMPRAS.md](COMPRAS.md) nuevo. Reescritura
de [PROTOCOLO.md](PROTOCOLO.md) con el protocolo verificado contra el datasheet. Regla 4 nueva en la
[política](POLITICA_DOCUMENTACION.md): las fuentes primarias se guardan dentro
del proyecto.

---

## 2026-08-02 — Se establece la documentación y se revisa el circuito de sonda

**Objetivo de la sesión:** definir cómo se va a documentar el proyecto, y
revisar el plan heredado antes de empezar a soldar.

**Estado del montaje al empezar:** nada montado. Solo documentación previa
(handover y plano de divisores).

### Qué se hizo

Sin trabajo de hardware. Dos cosas:

1. Se creó la estructura de `docs/` y la política de documentación.
2. Se revisó el cálculo de los divisores de tensión del plan heredado.

### Resultados

El divisor propuesto de 800 Ω / 1.72 kΩ **no sirve**. El cálculo de 3.42 V es
correcto solo si la línea fuera una fuente de tensión, y no lo es: el bus es
open-drain y sube a alto únicamente por un pull-up interno de la caja de
control. El divisor forma un segundo divisor con ese pull-up y hunde el nivel
alto del bus a ~1.75 V, muy por debajo del umbral del AiP650.

Consecuencia: el mando habría dejado de funcionar al conectar el ESP32, y el
síntoma habría apuntado a las soldaduras del conector, no a las resistencias.

Detalle completo en [ADR-005](DECISIONS.md).

Segundo hallazgo, del datasheet: el AiP650 es familia TM1650 y **no usa
direccionamiento I2C de 7 bits**. El primer byte tras el START es un comando
fijo, no una dirección. Registrado como [ADR-006](DECISIONS.md).

### Interpretación

- **Verificado por cálculo:** el divisor 800/1.72 k rompe el bus para cualquier
  pull-up por debajo de ~15 kΩ.
- **Supuesto:** que el pull-up es de 4.7 kΩ. Es el valor habitual pero no está
  medido, y determina qué sonda es viable. Se confirma midiendo resistencia
  entre una línea de datos y el hilo rojo, con el escritorio desenchufado.
- **Supuesto (heredado, sin confirmar):** que el hilo rojo es VCC y no una
  tercera línea de señal. Se confirma en la misma medición.

### Estado del montaje al terminar

Sin cambios. Nada soldado, nada conectado. El mando sigue intacto y funcional.

### Siguiente paso

Medir el pull-up del bus con el escritorio **desenchufado de la corriente**:
resistencia entre hilo amarillo y hilo rojo, y entre hilo verde y hilo rojo.
Es una medición sin riesgo y decide qué componentes hay que comprar.

### Adición del operador, mismo día

Contexto aportado en conversación, sin medición nueva:

- Marca comercial del escritorio: **Cougar**. Marca de periféricos, mecanismo
  maquilado; la pista útil sigue siendo Jiecang / TM1650.
- Los 5 pulsadores son **subir, bajar, M1, M2, reset**. Corrige el supuesto de
  ADR-003 de que el quinto era "M" de grabar preset. Registrado como
  [ADR-008](DECISIONS.md): el reset baja el escritorio hasta el tope para
  recalibrar, así que no se cablea nunca.
- Confirmado que el display muestra la altura. Es lo que sostiene ADR-001.

### Segunda adición del operador, mismo día

Comportamiento de los botones, verificado por uso:

- Pulso corto en subir/bajar → ≈ 1 cm. Mantener → hasta el límite. Cualquier
  botón detiene el movimiento.
- Mantener M1/M2 → **graba** la altura actual en ese preset.

Dos consecuencias, ambas registradas:

- [ADR-009](DECISIONS.md): con la altura leída del bus, los relés alcanzan
  altura arbitraria — mantener, vigilar, soltar al llegar. El modo relé deja de
  ser plan B degradado y pasa a ser solución completa. Inyectar en el bus queda
  como mejora opcional, no como requisito.
- [ADR-010](DECISIONS.md): el mismo botón que recupera un preset lo sobrescribe
  si se mantiene, y el fallo es silencioso. Los pulsos de M1/M2 se acotan por
  temporizador independiente del firmware.

También se precisó el papel del AiP650: es **esclavo**. La caja de control es el
master y le pregunta por el teclado; toda la lógica de tiempos vive en la caja.
Por eso inyectar una pulsación es responder a esa pregunta en lugar del chip.
Al ser open-drain solo se puede forzar un bit a 0, así que la viabilidad depende
de si la tecla pulsada se codifica con ceros o con unos. Anotado en
[PROTOCOLO.md](PROTOCOLO.md) como la pregunta que debe responder la captura.

Pendiente de medir, sin instrumentos: el umbral de tiempo que separa "ir al
preset" de "grabar preset".

### Investigación del chip y del mando, mismo día

A partir de fotografías macro del AiP650EO y de la serigrafía de la PCB, se
investigó el chip con fuentes públicas. Todo en [REFERENCIAS.md](REFERENCIAS.md).
Sin medición, sin tocar hardware.

Tres resultados, en orden de importancia:

**1. La inyección en el bus es imposible.** El byte de teclado del TM1650 marca
la pulsación con el **bit 6 a uno**; en reposo ese bit está a cero. Simularla
exige forzar un bit de 0 a 1, y el bus open-drain solo permite forzar a 0. No es
difícil: es eléctricamente imposible. [ADR-011](DECISIONS.md). El accionamiento
queda por contacto seco, definitivamente, y el bus pasa a ser de solo lectura
permanente en vez de "por ahora".

**2. El protocolo ya está documentado, no hay que descifrarlo.** Comandos
(`0x48` control, `0x68`/`0x6A`/`0x6C`/`0x6E` dígitos, `0x49` teclado), mapa de
segmentos bit a bit y formato del byte de tecla, todo en
[PROTOCOLO.md](PROTOCOLO.md). **El decodificador de altura se puede escribir
antes de capturar nada**; la captura pasa de descubrir a confirmar.

**3. El pinout del chip evita una incógnita entera.** Pines 5 = SCL, 6 = SDA,
15 = GND, 16 = VDD. Con continuidad entre los hilos del conector y esos pines se
identifican los cuatro hilos con multímetro — incluido cuál es SDA y cuál SCL,
que se iba a resolver escuchando el bus. Añadido como medición A en
[PLAN.md](PLAN.md).

Además:

- Riesgo nuevo detectado: si la caja apaga el display por inactividad, la altura
  se queda congelada y **parece válida**. [ADR-012](DECISIONS.md).
- `JK-CH506` no está en el catálogo público de Jiecang (sus mandos son
  `JCHT35Kxx`). Es referencia de fabricación; no hay documentación oficial que
  buscar, y la del chip es suficiente.
- Serigrafía releída sobre la foto: `G0088-30-4137-202518`, distinta de la
  transcripción del handover. El sufijo parece código de fecha, semana 18 de
  2025.
- Posible atajo sin explorar: si la caja de control tuviera un puerto serie
  adicional, hay componentes ESPHome ya hechos para el protocolo Jiecang y
  sobra todo lo demás. Añadido como comprobación C en [PLAN.md](PLAN.md).

Pendiente menor: guardar las fotografías macro del chip y de la serigrafía en
`hardware/`.

### Sonda final y lista de compras

Inventario medido pieza a pieza, 30 resistencias. Sonda fijada en
[ADR-016](DECISIONS.md): **9.1 kΩ arriba, 27 kΩ abajo**, bus a 3.99 V y GPIO a
2.99 V. Falta comprar solo **dos resistencias de 27 kΩ**.

Hallazgo aparte, al preparar el pedido: **el módulo de relés mecánicos del
inventario no sirve para el accionamiento.** Los pulsadores conmutan
microamperios —2 kΩ en serie en la matriz y pull-down interno de 50 µA— y un
contacto mecánico por debajo de su corriente mínima se vuelve intermitente con
el tiempo, con un modo de fallo que aparece meses después. Se sustituye por
relés de estado sólido optoacoplados (photoMOS). [ADR-017](DECISIONS.md).

Lista de compras razonada en [COMPRAS.md](COMPRAS.md), archivo nuevo. Lo de
mayor valor por euro que hay ahí: un **analizador lógico USB de 10 €**, que
elimina la restricción de partida del proyecto —"no hay instrumentos, el ESP32
tiene que hacer de instrumento"— y convierte la fase de captura de "a ciegas" en
"mirando".

### Archivos generados

Toda la estructura de `docs/`. ADR-001 a ADR-012, más REFERENCIAS.md. El handover original quedó
congelado en [historia/HANDOVER-2026-07-27.md](historia/HANDOVER-2026-07-27.md)
y el plano antiguo en
[hardware/plano_divisores_v1.svg](hardware/plano_divisores_v1.svg), marcado como
obsoleto por ADR-005.

---

## Sesiones anteriores a esta política — sin bitácora

El trabajo previo (identificación del mando, del chip AiP650EO, del pinout del
cable con multímetro, y el inventario de resistencias) se hizo antes de existir
esta política. No hay registro por sesión; el resultado consolidado está en
[historia/HANDOVER-2026-07-27.md](historia/HANDOVER-2026-07-27.md) y repartido
por los documentos temáticos.

Lo que **sí** se sabe de esas sesiones, y conviene recordar: la recomendación
osciló varias veces entre relés y cable sin dejar constancia de por qué, lo que
generó confusión y trabajo repetido. Esa es la razón de ser de la regla 1 de la
[política](POLITICA_DOCUMENTACION.md).
