# Bitácora

> Diario de sesiones. Lo más reciente arriba. Una entrada por sesión de trabajo,
> aunque no haya funcionado nada. Plantilla en
> [plantillas/entrada-bitacora.md](plantillas/entrada-bitacora.md).

---

## 2026-09-03 — El escritorio subió sin nadie delante, y fue por mi arreglo de ayer

> *"el sistema no está siendo preciso, hoy se levantó sin que yo estuviera, y el
> sensor marcaba que no había presencia"*

**Fallo de seguridad real, y lo introduje yo el día anterior.**

Reconstruido de los registros, sin margen de duda:

```
13:13:58  presencia CRUDA      off          <- deja de estar
13:16:00  presencia CRUDA      unavailable  <- el sensor ni responde
13:16:56  MOVIMIENTO           subiendo     <- el escritorio sube igual
13:17:40  ALTURA               116
13:28:58  presencia SOSTENIDA  off          <- doce minutos tarde
```

El 2026-09-02 moví la re-verificación previa al movimiento del sensor crudo a
`presencia_sostenida`, porque el crudo parpadea y cancelaba movimientos con el
propietario sentado delante. **Arreglé eso y abrí esto:** la sostenida tiene
`delay_off` de 15 minutos, así que dio por buena una presencia de hacía tres.

**Es peor que el fallo que sustituyó.** [SEGURIDAD.md](SEGURIDAD.md) exige
confirmación de que hay alguien delante; esto aceptaba una confirmación caducada
un cuarto de hora.

### El error de fondo: un sensor para dos preguntas distintas

| Pregunta | Tolerancia | Sensor |
|---|---|---|
| *¿Sigue siendo su turno de estar sentado?* | 15 min, un café no es un cambio de postura | `presencia_sostenida` |
| *¿Puedo mover un mueble AHORA?* | **3 min**, ni uno más | **`presencia_reciente`** ← nuevo |

Tres minutos siguen absorbiendo el parpadeo medido del mmWave (cae y vuelve cada
1-2 min) sin autorizar nunca un movimiento con la silla vacía. Las dos
automatizaciones de postura re-verifican ahora contra el nuevo sensor.

### Un segundo agujero, del mismo evento

El sensor pasó a **`unavailable`**, no a `off`. La automatización que frena el
escritorio si desapareces **a mitad de un movimiento** solo vigilaba `off`, así
que un sensor caído de la red le parecía "nada que hacer". Ahora vigila también
`unavailable` y `unknown`.

**Un sensor que no contesta no es prueba de que haya alguien.**

### Y el mismo día, el fallo contrario: frenó un viaje legítimo

Dos horas después, el propietario pulsó memoria 1 en HA y **el escritorio paró
en 111 en vez de llegar a 80**.

```
14:18:30  MEMORIA 1  -> la caja empieza a bajar de 117 hacia 80
14:18:38  presencia CRUDA -> off, durante DOS segundos
14:18:39  la automatizacion de parar dispara y publica `parar`
14:18:42  el escritorio se detiene en 111
14:18:40  el sensor vuelve a on, tarde
```

Estaba sentado delante todo el rato. Pulsó otra vez 45 s después y bajó los 29 cm
sin problema.

**Y lo agravó el arreglo de esa misma mañana:** añadir `unavailable` y `unknown`
al vigilante lo hizo más sensible, así que la capa que frena cuando te vas empezó
a frenar estando tú.

Arreglado con **confirmación de 10 s** (`for`) en el disparador. Sigue mirando el
sensor crudo, porque ahí la reacción rápida es el objetivo, pero ya no se cree un
parpadeo de dos segundos. **Coste declarado:** si la ausencia es real, frena 10 s
más tarde, unos 7 cm más de recorrido. Un viaje entero dura ~65 s.

### Las tres capas, ya separadas

| Capa | Pregunta | Sensor | Tolerancia |
|---|---|---|---|
| 1. Disparo | ¿Le toca cambiar de postura? | `presencia_sostenida` | 15 min |
| 2. Antes de mover | ¿Puedo mover un mueble AHORA? | `presencia_reciente` | 3 min |
| 3. Durante el viaje | ¿Se fue MIENTRAS se movía? | sensor crudo | 10 s |

**Tres preguntas, tres tolerancias, tres sensores.** Todos los fallos de esta
zona vinieron de hacer que un sensor respondiera a dos de ellas.

### Lo que esto dice del método

Los dos fallos de ayer y este son el mismo patrón: **un arreglo verificado
contra el síntoma que lo motivó, sin comprobar qué rompía en la otra
dirección.** Ayer verifiqué que el escritorio volviera a moverse; no verifiqué
que siguiera negándose a moverse sin nadie. Y esta mañana, arreglando eso, rompí
lo primero otra vez: dos horas después frenaba viajes legítimos. **Tres arreglos
seguidos, cada uno rompiendo la dirección que el anterior protegía.**

Aparte: el propietario cambió el sensor a un USB de la Pi y pregunté si eso
interfería. **Descartado midiendo** `zigbee.db`: LQI **199 contra el
coordinador** sobre 255, mejor que casi toda la red. No hay que recolocar nada.
Detalle en [INTEGRACION_HA.md](INTEGRACION_HA.md).

### Estado

Escritorio operativo. Firmware sin tocar. Tres sensores de presencia con
propósitos separados y escritos. Sin verificar todavía: que el nuevo sensor de 3
minutos no vuelva a cancelar movimientos legítimos. **Eso hay que observarlo un
par de días**, y es exactamente el error que acabo de cometer dos veces.

---

## 2026-09-02 — Dos veces tenía razón él y no le creí

> *"no es posible, me he estado moviendo mucho, el sensor no es"*

Sesión de diagnóstico. **Ningún cambio de hardware.** Dos fallos reales
encontrados en el lado de Home Assistant, los dos invisibles desde la interfaz.

### El sensor de presencia parpadea, y yo culpaba al propietario

Los recordatorios avisaban y luego **no movían el escritorio**. Mi explicación,
repetida dos veces, fue que el mmWave pierde a una persona quieta. El
propietario la rechazó las dos veces. Consultando la base de datos del
`recorder` resultó que **tenía razón**: el sensor crudo cae y vuelve cada 1-2
minutos, con él sentado delante y moviéndose. Decenas de transiciones en seis
horas, frente a **4** del sensor `presencia_sostenida`.

La re-verificación previa al movimiento preguntaba al sensor **crudo**. Cayó en
uno de esos huecos y canceló el movimiento. No era mala suerte: con ese
parpadeo, **cualquier comprobación instantánea es una moneda al aire**.

Arreglado: disparo y re-verificación usan `presencia_sostenida`.

Y un tercer hallazgo, del mismo tirón: llegué a anotar que el sensor *revertía
su retardo a 30 s solo*. **Falso, y lo corrijo aquí mismo.** Los registros
muestran que la automatización que lo pone en 120 **está desactivada desde el
2026-08-31 a las 21:03**; por eso lleva tres días en 30. Su alias además dice
*"a 30 s"* mientras su código pone 120 — el nombre viejo, sin actualizar. Queda
sin explicar solo una línea: por qué el valor cayó de 120 a 30 a las 20:58 de
aquel día, con la automatización aún activa.

### Cada reinicio de Home Assistant borraba las horas acumuladas

Detectado por el propietario en caliente: *"no va a subir porque dice que llevo
sentado 5 min"*. Yo acababa de reiniciar Home Assistant.

La automatización que empieza un periodo nuevo se disparaba con **cualquier**
cambio del sensor de postura — incluido el `unknown → sentado` de un arranque,
que no es levantarse sino el sensor naciendo. **Cada reinicio ponía el contador
a cero.** Corregido exigiendo que la postura de origen sea también real, y
**verificado reiniciando a propósito**: el contador conservó su valor.

El contador se devolvió a su valor real (10:04:51, la última ausencia larga de
verdad), leído de la base de datos, no estimado.

### Lo que esta sesión debería haber cambiado antes

Petición del propietario: *"no sé si tenemos logs de todo el sistema, para no ir
a ciegas con estas cosas"*. **Los había desde el principio** —30 días en SQLite—
y llevo la sesión entera emitiendo diagnósticos plausibles sin consultarlos. Los
dos hallazgos de arriba salieron en minutos en cuanto miré.

Queda escrito cómo consultarlos en
[INTEGRACION_HA.md](INTEGRACION_HA.md), con las tres trampas que me costaron
tiempo: abrir en `mode=ro`, no **inventar** los `entity_id` (inventé seis, los
seis falsos, y monté con ellos un informe entero de bloqueos imaginarios), y no
fiarse del `last_changed` de la interfaz tras un reinicio.

### La prueba de que el fallo era ese: el día entero, disparo a disparo

El propietario aportó el dato que lo cerró todo: *"la última vez que el
escritorio intentó subir, lo paré manualmente"*. Buscando ese momento en los
registros apareció la cronología completa del día. **Cada fila en punto es un
disparo; la de 110 s después es el final de la espera previa a mover:**

| Disparo | ¿Movió? | Con qué re-verificación |
|---|---|---|
| 07:15, 07:45, 08:20, 08:55 | **no** | sensor crudo |
| **09:30** | **sí** | sensor crudo (acertó) |
| 10:50 | **no** | sensor crudo |
| 11:25 | **no** | sensor crudo |
| 12:00 | no (cortado a los 34 s) | — |
| **12:30** | **sí** | **presencia sostenida** ✅ |

El de las 09:30 es el que el propietario paró con el mando: a las 09:31:54 el
sensor marca `subiendo` (orden del ESP32) y a las 09:31:55 `subiendo (mando)`,
con `uso_manual` a cero. **El sensor de movimiento hizo justo su trabajo**:
distinguir quién movió el escritorio.

Los de 10:50, 11:25 y 12:00 **completaron los 110 s de espera y luego no
movieron**. Ese es el fallo exacto: la re-verificación contra el sensor crudo,
cancelando en silencio. A las 12:30, con el arreglo puesto, movió.

⚠️ **Suelto, y no menor:** el propietario dice que *"no volvió a avisar nada"*,
pero la notificación se envía **antes** de esa espera, así que hoy salieron
**nueve avisos** desde las 07:15 y no vio ninguno. **No se pudo comprobar si
llegaron al móvil: los logs de esa franja se habían rotado** por los reinicios
de esta misma sesión. Queda pendiente, y es independiente del fallo de arriba.

### Verificado de extremo a extremo, por primera vez

A las 12:30 del 2026-09-02, con los dos arreglos puestos y **sin intervención
manual**:

```
12:30:00  dispara el recordatorio → notificación al móvil
12:31:50  (tras los 110 s) re-verifica la presencia sostenida → OK
12:31:57  subiendo            ← orden del ESP32, no del mando
12:32:41  frenando → 116 cm
12:32:46  117 cm
```

**El ciclo completo —avisar, esperar, re-verificar, mover— ocurrió entero.** Es
la primera vez que se observa así; los tres fallos anteriores lo cortaban en el
tercer paso.

⚠️ **Y una trampa de medición que casi me hace declarar un fallo inexistente:**
estuve leyendo `last_triggered` de la base de datos para saber si la
automatización había disparado. **Ese atributo se vuelca con retraso**, y decía
`None` incluso con el escritorio ya en 117. Llegué a concluir que "no ha
disparado nunca" con el escritorio subiendo. **Para saber si un recordatorio
funcionó, la fuente buena es el movimiento** (`sensor.…_movimiento` y la
altura), no el atributo de la automatización.

### Un riesgo que salió de una pregunta, no de una revisión

*"Mi preocupación es cuando se dispara una automatización de HA y yo lo paro a
mano. Si no hay riesgo con eso lo dejamos así."*

Lo hay, y no estaba evaluado. **Eléctricamente no** —el optoacoplador está en
paralelo con el pulsador, cerrar ambos es pulsar una tecla con dos dedos— pero
el pulso largo **mantiene el contacto cerrado 2800 ms sin poder abortarse**, y
en esa ventana el firmware es ciego a una pulsación humana. Si se pulsa la tecla
contraria, la caja ve **SUBIR y BAJAR a la vez**, y lo que hace con eso **no
está verificado**.

Esa misma mañana el propietario pulsó la tecla contraria **~200 ms después** de
que el contacto se abriera. Detalle, mitigaciones y la comprobación pendiente en
[SEGURIDAD.md](SEGURIDAD.md).

**Sin decidir:** usar M1/M2 para los objetivos de postura reduciría la ventana
de 2800 a 800 ms y dejaría el frenado en manos de la caja —que también resuelve
el riesgo de ADR-028, un cuelgue del ESP32 en viaje continuo—. Contradice la
decisión de no acoplar las memorias al sistema, así que **necesita ADR**. El
propietario cerró la sesión antes de decidir.

### Traducir obligó a releer, y releer encontró tres mentiras

El propietario pidió pasar la documentación de cara al público a inglés. Efecto
lateral no buscado: **traducir obliga a leer cada frase**, y aparecieron tres
afirmaciones falsas que llevaban semanas ahí.

**1. `HARDWARE.md` decía, en su última línea:** *"ESP32 por USB desde el Mac. El
hilo rojo del escritorio no se conecta."* **El rojo es CLK y sí se conecta**,
por su divisor. El que no se conecta nunca es el **amarillo**, que son los 5 V.
El resto del documento lo dice bien en cuatro sitios —la tabla de pinout, el
punto de derivación, el plano de la sonda y la corrección del handover— y esa
línea sola los contradecía a todos.

Es exactamente el error que la [política](POLITICA_DOCUMENTACION.md) pone como
ejemplo de por qué existe la regla 5. Y es más peligroso de lo que parece: quien
lo leyera y actuara al revés conectaría **5 V a un GPIO de 3.6 V**.

**2. `firmware/README.md` decía:** *"Qué está sin verificar: todo. Este firmware
está escrito contra el datasheet, no contra el bus real. Nunca se ha ejecutado
con hardware conectado."* Llevaba **desde el 2026-08-06** siendo falso, con el
firmware gobernando el escritorio.

**3. `INTEGRACION_HA.md` describía la postura con un umbral único en 95 cm**,
dos secciones más abajo de la sección que explica por qué ese umbral se eliminó
el 2026-08-28.

**El patrón es el mismo en los tres:** se corrigió algo en un sitio y no se
releyó el documento entero. Ninguna de las tres es un descuido de escritura; las
tres son documentos que envejecieron sin que nada obligara a revisarlos.

Todas corregidas dejando el texto viejo citado, como pide la política.

### El repositorio pasa a dos idiomas

La documentación de cara al exterior queda en inglés; **la bitácora y los ADR se
quedan en español**, y eso es deliberado: son un diario escrito mientras pasaban
las cosas, con la distinción entre medido y supuesto incrustada en el modo de
decirlo, y los ADR además son inmutables —traducir es editar—. La regla queda
escrita en [POLITICA_DOCUMENTACION.md](POLITICA_DOCUMENTACION.md) y la tabla del
README dice el idioma de cada documento.

Los nombres de archivo no se renombran aunque el contenido esté en inglés:
romperían los enlaces desde la bitácora y los ADR, que no se tocan.

### Estado

Escritorio operativo. Firmware sin tocar. Contador de postura restaurado y
sobreviviendo a reinicios. Retardo del sensor **en 30 s, sin tocar**, a petición
del propietario.

---

## 2026-08-31 — El sensor de presencia perdía a una persona quieta

> *"se supone que el escritorio debió bajar hace como 5 min"* … *"pero si estaba,
> no me he movido de aquí"*

**El recordatorio funcionó y aun así no bajó**, y la culpa era de un ajuste mío.

Reconstruido de los registros:

| Hora | Qué pasó |
|---|---|
| 09:12 | Sube a 117 → empieza a contar "de pie" |
| **09:45** | La automatización dispara y avisa. Correcto |
| ~09:47 | Re-verifica presencia → **el sensor dice que no hay nadie** → se detiene |
| 09:53 | El sensor vuelve a detectarle |

**Pero él estaba ahí todo el rato.**

### La causa: bajé el retardo del sensor a 30 s

El 2026-08-23 bajé el retardo ocupado→desocupado del SNZB-06P de 90 a 30 s para
que detectara antes las ausencias. **Solo pesé una cara del ajuste.** La otra es
que un mmWave sigue a una persona por la respiración y los micro-movimientos: con
30 s, alguien trabajando muy quieto **desaparece del sensor**. Con 90 s no
llegaba a notarse.

**Y el fallo era silencioso**: la automatización se cancelaba "correctamente"
creyendo que no había nadie, sin dejar nada raro en ningún registro.

### Arreglado en dos frentes

1. **Retardo a 120 s**, más generoso que el original. La detección de ausencia
   real durante un movimiento la cubre la protección de presencia, no este valor
2. **El disparo usa la presencia SOSTENIDA** (`delay_off` de 15 min) en vez del
   sensor crudo: una pérdida momentánea ya no cancela un recordatorio. La
   re-verificación antes de mover sigue con el sensor crudo, que es donde
   interesa la respuesta inmediata

### Y la regla de cortesía tampoco protegía nada

El sensor `uso_manual` —segundos desde la última vez que alguien tocó el mando—
**solo se publicaba después de la primera pulsación desde el arranque**. Con los
reinicios de las actualizaciones, la entidad no existía, y la condición que la
consulta *"no muevas el escritorio si se ajustó a mano hace menos de 5 min"*
**no estaba protegiendo nada**.

**Arreglado**: se publica siempre. Si nadie lo ha tocado, vale `99999` — que no
es un dato ausente, es "hace muchísimo". Desplegado por OTA y verificado.

**Es el mismo patrón que ya mordió dos veces en este proyecto**: una condición de
seguridad que, al no poder evaluarse, deja de proteger en silencio en vez de
avisar.

### Cinco automatizaciones estaban desactivadas

Al revisarlo aparecieron apagadas: las tres de aviso (`desconectado`, `volvió`,
`bus degradado`), el `resumen cada 30 min` —del que ya se había advertido que
acabaría cansando— y ⚠️ **`parar si desaparece la presencia`**, que no es un
aviso sino **una protección**: es la capa que detiene el escritorio si te vas
mientras se mueve.

**Reactivada a petición del propietario.** Y con un matiz que la explica: **hasta
el 2026-08-24 esa automatización no podía funcionar** —el sensor de movimiento no
se actualizaba durante los viajes—, así que apagarla por inútil era razonable.
Ahora sí funciona.

Las otras cuatro quedan apagadas a propósito.

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

## Entradas anteriores al 2026-08-22 — archivadas

Las 11 entradas de las fases 1 a 3 (del 2026-08-02 al 2026-08-22) están en
[historia/BITACORA-fases-1-3.md](historia/BITACORA-fases-1-3.md), **sin editar
ni recortar**. Se movieron el 2026-09-02, cuando este archivo llegó a 3531
líneas.

Cubren la identificación de los cuatro hilos, el descifrado del bus, el montaje
de la sonda y la verificación de los cuatro canales. Nada de eso se ha perdido:
lo que sigue siendo cierto vive en los documentos temáticos, y las decisiones en
[DECISIONS.md](DECISIONS.md).
