# Registro de decisiones (ADRs)

> Registro inmutable. Cada entrada explica el contexto, la decisión y qué se
> pierde al tomarla. Una entrada publicada no se modifica: si la decisión
> cambia, se añade una nueva que la reemplaza.

Estados posibles: `Activa`, `Reemplazada por ADR-NNN`, `Bloqueada`.

---

## ADR-001 — La ruta principal es el bus de datos del mando, no los pulsadores

**Fecha:** 2026-08-02 (retroactivo, decidido en sesión previa)
**Estado:** Activa

**Contexto:** Hay dos formas de automatizar el escritorio. Accionar los
pulsadores del mando con relés es lo más directo. Pinchar el bus de 4 hilos
entre la caja de control y el mando es más difícil pero da lectura.

Sin conocer la altura real, automatizar se reduce a contar segundos, y eso
deriva: la velocidad varía con el peso sobre la superficie, el motor arranca
con rampa, y si alguien mueve el escritorio con el mando el ESP32 no se entera.

**Decisión:** Ir por el bus. El AiP650EO del mando recibe por el bus los dígitos
que muestra el display, así que escuchándolo se obtiene la altura literal — la
misma que se lee en pantalla. El mismo chip escanea el teclado, de modo que las
pulsaciones también viajan por ahí.

**Qué se pierde:** Un camino mucho más largo. Los relés funcionarían esta misma
tarde; descifrar un protocolo propietario puede no funcionar nunca. Se acepta
porque el lazo cerrado es el requisito real, no un lujo.

---

## ADR-002 — Fase de solo lectura antes de cualquier inyección

**Fecha:** 2026-08-02 (retroactivo)
**Estado:** Activa — reforzada por [ADR-011](#adr-011): la lectura ya no es una
fase, es permanente, porque inyectar resultó ser imposible

**Contexto:** El ESP32 podría escribir en el bus para simular pulsaciones. Pero
el mando y el ESP32 compartirían el medio, con riesgo de colisión, y un error
de escritura puede dejar el escritorio inutilizable.

**Decisión:** La fase actual es **solo escucha, en paralelo**. El mando queda
conectado y funcional, no se corta ningún hilo. Inyectar se evalúa después, y
solo si el protocolo se descifra.

**Qué se pierde:** No hay control alguno hasta terminar la ingeniería inversa.
A cambio, en todo momento el escritorio sigue usable con su mando original y
cualquier fallo del ESP32 es inocuo.

---

<a id="adr-003"></a>

## ADR-003 — Los relés se conservan como plan B de accionamiento

**Fecha:** 2026-08-02 (retroactivo)
**Estado:** Activa — supuesto corregido por [ADR-008](#adr-008), coste corregido
por [ADR-009](#adr-009)

**Contexto:** Leer e inyectar son problemas distintos. Es plausible descifrar la
lectura de altura y no lograr escribir en el bus.

**Decisión:** Si leer funciona pero inyectar no, el montaje es híbrido: altura
por el bus, accionamiento por relés en paralelo a los pulsadores. Los relés
actúan como contacto seco, lo que aísla eléctricamente y hace irrelevantes los
5 V que hay entre las patas de los pulsadores.

Bastan 4 relés: subir, bajar y dos memorias. En modo relé se accionan **las
memorias del escritorio**, no se temporiza — la caja de control ya sabe llegar
a una altura exacta por sí sola, y esa capacidad es gratis.

El quinto pulsador es probablemente "M" (grabar preset) y no se automatiza:
un preset sobrescrito por accidente rompe todas las posiciones guardadas.

**Qué se pierde:** En modo híbrido solo se alcanzan las posiciones guardadas
como preset, no una altura arbitraria.

---

## ADR-004 — Los pulsadores no se conectan a un GPIO

**Fecha:** 2026-08-02 (retroactivo)
**Estado:** Activa

**Contexto:** Medido entre las patas de un pulsador del mando: **5 V**. Los
GPIO del ESP32 toleran 3.6 V como máximo absoluto.

**Decisión:** Ninguna pata de pulsador toca un GPIO, ni para leer ni para
accionar. Si se accionan, es a través de los relés (ADR-003).

**Qué se pierde:** Nada de valor. Es una restricción física, no una preferencia.

---

## ADR-005 — El divisor 800 Ω / 1.72 kΩ queda descartado: carga demasiado el bus

**Fecha:** 2026-08-02
**Estado:** Resuelta por [ADR-013](#adr-013) — el pull-up no se midió, lo da el
datasheet del AiP650E
**Reemplaza a:** el cableado descrito en
[historia/HANDOVER-2026-07-27.md](historia/HANDOVER-2026-07-27.md) y en
[hardware/plano_divisores_v1.svg](hardware/plano_divisores_v1.svg)

**Contexto:** El plan original bajaba los 5 V del bus a nivel seguro con un
divisor de 800 Ω arriba y 1.72 kΩ abajo por cada línea de datos. Aritméticamente
da 3.42 V, dentro de rango, y se eligió porque hay dos pares completos en el
inventario.

El error es que el cálculo trata la línea como una fuente de tensión, y no lo
es. El bus es **open-drain**: nadie lo empuja hacia arriba, sube solo por un
pull-up hacia 5 V que vive dentro de la caja de control. El divisor no cuelga de
una fuente, cuelga de ese pull-up, y forma un segundo divisor con él.

Cada divisor presenta 800 + 1720 = **2.52 kΩ permanentes a GND**. Si el pull-up
es de 4.7 kΩ, valor habitual, el nivel alto del bus cae a:

```
5 V x 2520 / (2520 + 4700) = 1.75 V
```

El AiP650 funciona a 5 V y necesita alrededor de 3.5 V para leer un "1". A
1.75 V el bus queda permanentemente en estado bajo desde su punto de vista: el
mando dejaría de responder en cuanto se conecte el ESP32. Peor aún, el fallo
apuntaría a las soldaduras del conector y no a las resistencias, que es donde
realmente estaría.

**Decisión:** El divisor de 800/1.72 k no se monta. La sonda debe ser de alta
impedancia. Un divisor de **10 kΩ / 20 kΩ** carga 30 kΩ, deja el nivel alto en
~4.3 V con un pull-up de 4.7 kΩ, y entrega ~2.9 V al GPIO — por encima del
umbral del ESP32. Alternativa mejor: un adaptador de nivel bidireccional con
BSS138, que casi no carga la línea.

Ninguna de las dos se puede montar todavía con el inventario actual: solo hay
una resistencia de 8.2 kΩ y ninguna cercana a 10 k o 20 k.

**Antes de comprar nada hay que medir el pull-up real.** Con el escritorio
desenchufado, la resistencia entre una línea de datos y la línea roja es
directamente el valor del pull-up. Ese número decide cuánta carga es tolerable
y si un divisor basta o hace falta el buffer.

**Qué se pierde:** El montaje se retrasa hasta conseguir componentes. Se acepta
sin discusión: la alternativa es un mando muerto y un fallo que apunta al sitio
equivocado.

---

## ADR-006 — El decodificador no asume I2C estándar

**Fecha:** 2026-08-02
**Estado:** Activa

**Contexto:** El AiP650EO es de la familia del TM1650. Estos chips usan un
protocolo *parecido* a I2C — mismas señales, mismos START/STOP, mismo ACK —
pero **sin direccionamiento de 7 bits**: el primer byte no es "dirección +
bit R/W", es un byte de comando fijo que selecciona display o teclado.

Un decodificador que asuma el direccionamiento estándar interpretará ese primer
byte como una dirección inventada y desalineará todo lo que venga después.

**Decisión:** El sniffer captura y vuelca **bytes crudos con su ACK/NACK**,
delimitados por START y STOP, sin interpretar el primer byte como dirección.
La interpretación se hace después, sobre los datos, en
[PROTOCOLO.md](PROTOCOLO.md).

**Qué se pierde:** La salida es menos legible de entrada. Se gana no construir
la decodificación sobre un supuesto que probablemente sea falso.

---

<a id="adr-007"></a>

## ADR-007 — El ESP32 se alimenta por USB durante el sniffing

**Fecha:** 2026-08-02 (retroactivo)
**Estado:** Activa — ampliada por [ADR-019](#adr-019), que añade el orden de
conexión: el USB va **antes** que los hilos del bus

**Contexto:** Sería cómodo alimentar el ESP32 desde los 5 V del propio
escritorio (el hilo rojo).

**Decisión:** Alimentación por USB desde el Mac. El hilo rojo no se conecta a
nada. Solo se comparte GND (hilo azul), que es imprescindible como referencia
común para poder medir.

**Qué se pierde:** Un cable más. A cambio, el consumo del ESP32 y sus picos de
arranque nunca perturban la fuente de la caja de control, y un cortocircuito
del lado del ESP32 no alcanza al escritorio.

---

<a id="adr-008"></a>

## ADR-008 — El quinto pulsador es Reset, y no se automatiza jamás

**Fecha:** 2026-08-02
**Estado:** Activa
**Corrige un supuesto de:** ADR-003 (cuya decisión de fondo sigue vigente)

**Contexto:** ADR-003 suponía que el quinto botón era "M" de grabar preset.
Es incorrecto. Los cinco botones son **subir, bajar, M1, M2 y reset**.

Cambia el riesgo, y a peor. Un "grabar preset" accidental estropea una posición
guardada, molesto pero reversible. El **reset** de estos escritorios ejecuta una
recalibración: baja la superficie hasta el tope inferior, sin pedir confirmación
y sin poder interrumpirse. Con una silla, un cajón o un gato debajo, es el peor
movimiento posible — y es justo el que un bug de direccionamiento de relés
podría disparar solo.

**Decisión:** Ningún relé, ni ninguna inyección de comando, alcanza el reset.
En el montaje híbrido se cablean **cuatro** botones — subir, bajar, M1, M2 — y
el reset se deja físicamente fuera del circuito. No es una comprobación por
software que se pueda saltar: el cable no existe.

Corolario: si en fase 3 se logra inyectar comandos en el bus, el código de tecla
del reset se identifica **para poder excluirlo explícitamente**, no para usarlo.

**Qué se pierde:** La recalibración queda como operación exclusivamente manual.
Que es donde debe estar: requiere que alguien mire debajo del escritorio.

---

<a id="adr-009"></a>

## ADR-009 — El modo relé alcanza altura arbitraria: deja de ser plan B degradado

**Fecha:** 2026-08-02
**Estado:** Activa — **pero el mecanismo descrito abajo es incorrecto y está
corregido por [ADR-023](#adr-023)**: "soltar al llegar" no para el escritorio.
La decisión de fondo (el contacto seco en lazo cerrado alcanza cualquier altura)
sigue siendo cierta, encadenando toques
**Corrige el coste declarado en:** [ADR-003](#adr-003)

**Contexto:** ADR-003 aceptaba que en modo relé "solo se alcanzan las posiciones
guardadas como preset, no una altura arbitraria". Con el comportamiento real de
los botones verificado, eso es falso:

- Pulso corto en subir/bajar → mueve ≈ 1 cm.
- Mantener → mueve hasta el límite.
- Cualquier botón → detiene el movimiento en curso.

Combinado con la altura leída del bus, el lazo se cierra sin inyectar nada:
mantener subir, vigilar la altura en el display, soltar al llegar, y afinar con
pulsos de ~1 cm. El relé no está temporizando a ciegas — está reaccionando a una
medición real, que es justo lo que ADR-001 exigía.

**Decisión:** El modo relé se considera una solución **completa**, no un
degradado. Inyectar en el bus pasa a ser una mejora opcional (menos hardware,
menos ruido mecánico), no un requisito del proyecto.

Los presets M1/M2 siguen siendo la vía preferente cuando el destino coincide con
uno: la caja de control llega sola y con su propia rampa, sin que el ESP32
tenga que frenar a tiempo.

**Consecuencia de seguridad:** "cualquier botón detiene" convierte cualquier relé
en freno de emergencia. El control de movimiento debe poder abortar siempre,
y ante cualquier lectura de altura incoherente, abortar es la acción por defecto.

**Qué se pierde:** Precisión limitada por el paso de ~1 cm del pulso corto y por
la rampa de frenado del motor al soltar un movimiento largo. Para un escritorio
es de sobra.

---

<a id="adr-010"></a>

## ADR-010 — Los pulsos de M1/M2 se acotan por hardware, no solo por software

**Fecha:** 2026-08-02
**Estado:** Activa

**Contexto:** Mantener M1 o M2 **graba** la altura actual en ese preset. Es el
mismo botón que sirve para recuperarlo, distinguido solo por el tiempo que se
mantiene cerrado.

Un relé que cierre de más no falla en ir a la posición: la sobrescribe. Y el
fallo es silencioso — el escritorio se queda donde está, que es exactamente lo
que un observador esperaría de "ir a una posición ya alcanzada". Se descubre
días después, cuando el preset lleva tiempo apuntando a otro sitio.

Las causas de un cierre largo no son solo un bug: un reinicio del ESP32 con el
GPIO en el estado equivocado, un cuelgue del firmware con el relé activo, o un
contacto pegado.

**Decisión:**

1. **Medir primero** el umbral que separa "ir a" de "grabar", y documentarlo en
   [HARDWARE.md](HARDWARE.md). Sin ese número no se cablea M1 ni M2.
2. Los pulsos de M1/M2 se emiten muy por debajo del umbral, con margen amplio.
3. El corte no depende solo del bucle principal: se acota con un temporizador
   independiente que abra el relé aunque el firmware esté colgado.
4. Verificar el estado de reposo de los GPIO de relé **durante el arranque y el
   reset** del ESP32, antes de conectarlos a nada. Si el módulo de relés es
   activo-bajo, un GPIO flotante en reset cierra el relé.

**Qué se pierde:** Una vuelta más de verificación antes de cablear el
accionamiento. Barato frente a perder los presets sin enterarse.

---

<a id="adr-011"></a>

## ADR-011 — Inyectar pulsaciones en el bus es imposible: el accionamiento es por contacto seco

**Fecha:** 2026-08-02
**Estado:** Activa
**Cierra la pregunta abierta de:** ADR-002 y [ADR-009](#adr-009)

**Contexto:** Quedaba abierto si el ESP32 podía simular una pulsación
respondiendo al master en lugar del AiP650. La viabilidad dependía de una sola
cosa: si "tecla pulsada" se codifica con ceros o con unos.

El datasheet del TM1650 lo responde. El byte de teclado usa el **bit 6 a uno**
para indicar pulsación; en reposo ese bit está a cero y el valor queda por
debajo de `0x40`. Simular una pulsación exige forzar un bit de 0 a 1.

El bus es open-drain: los participantes solo pueden **tirar hacia abajo**, el
nivel alto lo pone el pull-up. Forzar un bit a 1 no es difícil, es
eléctricamente imposible sin pelearse con el chip.

Lo único que sí se puede hacer —tirar de SDA abajo— **enmascara** una pulsación
real. No sirve para nada aquí.

**Decisión:** No se inyecta en el bus. El bus es **exclusivamente de lectura**,
de forma permanente y no como fase transitoria. El accionamiento es por contacto
seco en paralelo a los pulsadores: relé, optoacoplador o interruptor analógico.
Los relés que ya hay en el inventario sirven.

Queda **prohibido** forzar SDA a alto con una salida push-pull para intentarlo:
mientras el AiP650 tira de la línea abajo, eso es un cortocircuito contra la
salida del chip. El chip es irreemplazable y el mando entero con él.

**Qué se pierde:** Hace falta hardware de accionamiento y cablear cuatro
pulsadores, en vez de resolverlo con los mismos dos hilos que ya se leen. Sale
gratis en capacidad: [ADR-009](#adr-009) ya establece que el contacto seco en
lazo cerrado alcanza cualquier altura. La ruta descartada no daba nada que la
elegida no dé.

**Efecto lateral bueno:** ADR-002 exigía fase de solo lectura por miedo a
romper el escritorio al escribir. Al no escribir nunca, ese riesgo desaparece
del proyecto entero, no solo de la fase actual.

---

<a id="adr-012"></a>

## ADR-012 — La altura leída se trata como dato con caducidad, no como verdad

**Fecha:** 2026-08-02
**Estado:** Activa

**Contexto:** El ESP32 conoce la altura porque ve pasar el refresco del display.
Si la caja de control apaga el display por inactividad —el TM1650 lo permite con
el comando `0x48`, y el chip está pensado justo para eso— el refresco se
detiene y la última altura vista se queda congelada.

No hay forma segura de forzar el refresco: los cinco botones hacen algo. Subir y
bajar mueven, M1 y M2 mueven o graban, y el reset no se toca jamás
([ADR-008](#adr-008)). No existe un botón inocuo con el que "despertar" el
display.

El caso peligroso no es que la lectura falte, es que **parezca válida**. Si
alguien mueve el escritorio con el display ya apagado, o si la caja se
reinicia, el ESP32 sigue creyendo un número viejo — y ese número es la
referencia con la que decide cuándo frenar.

**Decisión:** La altura se guarda con marca de tiempo del último refresco visto.
Pasado un umbral sin refresco se marca **obsoleta**, y con altura obsoleta no se
inicia ningún movimiento automático. Se recupera esperando a ver refresco real,
no suponiendo.

Durante un movimiento en curso, si el refresco se detiene o salta de forma
incoherente, se frena. Ver [SEGURIDAD.md](SEGURIDAD.md).

**Qué se pierde:** Puede haber ratos en que Home Assistant muestre la altura
como "desconocida" en vez de un número. Es correcto: era desconocida.

**Nota:** el escenario se vuelve inofensivo si resulta que la caja refresca el
display de forma indefinida. Es la pregunta 2 de [PROTOCOLO.md](PROTOCOLO.md) y
la captura la responde.

**Respuesta, 2026-08-06: el bus NO se calla.** Medido con el escritorio quieto
durante **15 minutos**: el sniffer se armó **4505 veces y ninguna encontró el bus
en silencio**. La caja sigue refrescando cada 200 ms indefinidamente, y el
comando de control siempre dice `sleep=no` — nunca ha mandado el bit de sueño.

Lo que sí ocurre a los pocos segundos es que **el display se apaga escribiendo
`0x00` en los cuatro dígitos**. Son dos cosas distintas, y solo pasa la primera.

**Esto reduce mucho el alcance de este ADR sin anularlo:**

- **La pérdida de refresco por sueño del chip deja de ser un escenario esperado.**
  No se ha observado en 15 minutos de inactividad.
- **La marca de tiempo y el criterio de caducidad siguen siendo necesarios**, por
  lo demás: la caja puede reiniciarse, el cable puede soltarse, la sonda puede
  fallar. Un bus que calla sigue significando "no sé la altura".
- **Y aparece una señal mejor de la que este ADR esperaba:** como el bus sigue
  vivo aunque la pantalla esté en blanco, el ESP32 puede distinguir **"no hay
  altura que mostrar"** de **"no hay bus"**. Eso era justo lo que hacía falta
  para no confundir un display apagado con un sistema muerto.

*Sin verificar: qué pasa tras horas de inactividad, o si la caja se comporta
distinto al despertar de un corte de corriente.*

---

<a id="adr-013"></a>

## ADR-013 — La sonda es un divisor 15 kΩ / 33 kΩ, calculado con el pull-up del datasheet

**Fecha:** 2026-08-02
**Estado:** Reemplazada por [ADR-016](#adr-016), y a su vez por
[ADR-022](#adr-022). **Ojo al razonamiento de abajo sobre los pull-ups externos:
es incorrecto.** Este ADR afirma que un pull-up más fuerte "solo puede jugar a
favor"; la medición del 2026-08-06 demostró que juega a favor del bus y **en
contra del GPIO**, y por poco deja el pin a un 2% de su máximo absoluto. La
impedancia declarada abajo (10.3 kΩ) está corregida por [ADR-018](#adr-018), que
además re-ancla el plan B del buffer que este ADR declara
**Resuelve:** [ADR-005](#adr-005), que quedó bloqueada a la espera de medir el
pull-up

**Contexto:** ADR-005 descartó el divisor de 800 Ω / 1.72 kΩ por cargar
demasiado el bus, y dejó el montaje bloqueado hasta medir el pull-up real. Esa
medición ya no hace falta: **el datasheet del AiP650E la da**.

Página 5: CLK es entrada con *built-in pull-up resistors* y DIO es
*N-Channel, Open-Drain* también con pull-up interno. Página 6 cuantifica ambos:
`IUP1` e `IUP2` = **550 µA típicos**, que a 5 V equivalen a unos **9.1 kΩ**.

Ese es el peor caso. El circuito de aplicación recomendado añade además dos
resistencias externas de 10 kΩ a VDD; si esta placa las lleva, el pull-up
resultante baja a ~4.8 kΩ, o sea **más fuerte**. Dimensionar para 9.1 kΩ es
seguro exista lo que exista fuera del chip.

Restricciones a cumplir, todas del datasheet:

- El nivel alto del bus debe quedar por encima de **VIH = 0.7 × VDD = 3.5 V**.
- El GPIO del ESP32 necesita al menos ~2.5 V para leer un 1, y no tolera más
  de 3.6 V.

**Decisión:** divisor de **15 kΩ arriba y 33 kΩ abajo** en cada línea de datos.

| | Valor | Margen |
|---|---|---|
| Carga sobre el bus | 48 kΩ | |
| Nivel alto del bus | 4.20 V | 0.7 V sobre los 3.5 V exigidos |
| Tensión en el GPIO | 2.89 V | sobre 2.5 V, y 0.7 V por debajo del máximo |
| Impedancia vista por el GPIO | 10.3 kΩ | |

Hacen falta **dos de 15 kΩ y dos de 33 kΩ**. Ninguna está en el inventario
actual; hay que comprarlas. Se monta en rojo (CLK) y verde (DIO); el amarillo
(5 V) no se conecta y el azul va a GND.

**Qué se pierde:** un divisor resistivo redondea los flancos. Con 10.3 kΩ de
impedancia y unos 30 pF de capacidad parásita, la constante de tiempo ronda
0.3 µs. Sobra para un bus de decenas de kHz, que es lo normal en esta familia,
pero el chip admite hasta 4 Mbps y **no sabemos a qué velocidad corre este**.

Si la captura sale sucia o con bits perdidos, la solución no es bajar las
resistencias —eso carga el bus y nos devuelve al problema de ADR-005— sino un
**buffer no inversor alimentado a 3.3 V con entradas tolerantes a 5 V**, tipo
74LVC2G17. Carga el bus con microamperios en vez de con 48 kΩ y no redondea
nada. Se deja como plan B declarado, no como improvisación.

**Nota:** la medición del pull-up que estaba pendiente en PLAN.md deja de ser
bloqueante. Sigue siendo informativa —diría si existen los 10 kΩ externos— pero
no cambia esta decisión en ninguno de los dos resultados posibles.

---

<a id="adr-014"></a>

## ADR-014 — Montaje provisional de la sonda con el inventario, con verificación obligatoria

**Fecha:** 2026-08-02
**Estado:** **Anulada por [ADR-015](#adr-015)** — se apoyaba en dos resistencias
de 15 kΩ que resultaron ser de 75 kΩ. Nunca se montó.
**Complementa a:** [ADR-013](#adr-013), que sigue siendo el diseño preferente

**Contexto:** ADR-013 fija la sonda en 15 kΩ / 33 kΩ. No hay resistencias de
33 kΩ y no se pueden comprar de inmediato. Confirmadas con multímetro dos
resistencias de **9.1 kΩ**, aparece una combinación que sí se puede montar hoy
repartiendo el material entre las dos líneas:

| Línea | Arriba | Abajo | Total |
|---|---|---|---|
| Rojo (CLK) | 9.1 kΩ | 15 kΩ + 7.5 kΩ = 22.5 kΩ | 31.6 kΩ |
| Verde (DIO) | 9.1 kΩ | 15 kΩ + 3.3 kΩ + 1.8 kΩ + 1.8 kΩ = 21.9 kΩ | 31.0 kΩ |

| | Rojo | Verde | Exigido |
|---|---|---|---|
| Nivel alto del bus | 3.88 V | 3.87 V | ≥ 3.5 V |
| Tensión en el GPIO | 2.76 V | 2.73 V | ≥ 2.5 V |
| Impedancia vista por el GPIO | 6.5 kΩ | 6.5 kΩ | — |

Tiene una ventaja real sobre el diseño de ADR-013: la impedancia baja de
10.3 kΩ a 6.5 kΩ, así que **los flancos se redondean menos** y aguanta un bus
más rápido.

Y una desventaja real: el margen sobre el umbral del chip cae de 0.7 V a 0.38 V.
El datasheet especifica el pull-up interno como **típico** (550 µA) sin dar
mínimo. Si el chip real tirase con 400 µA, este montaje dejaría el bus en
3.58 V —rozando el umbral de 3.5 V— mientras que el de ADR-013 lo dejaría en
3.97 V.

**Decisión:** se monta esta combinación **con verificación obligatoria**, no a
ciegas:

1. **Antes de conectar nada**, medir con el escritorio encendido el voltaje de
   rojo↔azul y verde↔azul. Anotar. Deberían rondar los 5 V.
2. Conectar la sonda y **volver a medir los mismos dos puntos**.
   - ~3.9 V → correcto, seguir.
   - por debajo de **3.6 V** → desconectar la sonda. El pull-up real es más
     débil de lo típico y hace falta el diseño de ADR-013.
3. Comprobar que el mando sigue funcionando: display estable y botones que
   responden. Si parpadea o falla, desconectar.

El riesgo es aceptable porque **el fallo es reversible y visible**: cargar el
bus de más no daña nada, solo impide que el chip lea unos y otros; se
manifiesta al instante y se deshace desconectando.

Las dos resistencias de 33 kΩ se compran igualmente. Este montaje es para no
quedarse parado, no para quedarse.

**Qué se pierde:** margen eléctrico y la necesidad de una comprobación que con
ADR-013 sería una simple formalidad. A cambio, el proyecto avanza hoy en vez de
esperar a un pedido.

---

<a id="adr-015"></a>

## ADR-015 — Se anula el montaje provisional y los valores se miden, no se leen

**Fecha:** 2026-08-02
**Estado:** Activa
**Anula:** [ADR-014](#adr-014)

**Contexto:** ADR-014 montaba la sonda con el inventario, usando dos
resistencias de 15 kΩ. Esas dos resistencias son de **75 kΩ**: la banda leída
como marrón era violeta.

Con 75 kΩ no hay recambio. Emparejadas con las 9.1 kΩ dan 4.0 V en el GPIO, por
encima de lo que tolera el ESP32, y corregir el reparto exige más material del
que hay. No existe combinación de dos divisores con el inventario real.

Es la tercera corrección de inventario del día, y las tres han cambiado el
diseño:

| Se leyó | Es | Consecuencia |
|---|---|---|
| 920 Ω | 9.1 kΩ | Desbloqueó ADR-014 |
| naranja-azul-café, 360 Ω | naranja-azul-negro, 36 Ω | Sin consecuencia |
| marrón-verde-naranja, 15 kΩ | violeta-verde-naranja, 75 kΩ | Anula ADR-014 |

La causa no es descuido: el código de colores **no tiene redundancia**. Un color
mal leído mueve el valor un factor de mil, y marrón contra violeta en una banda
pequeña con luz de escritorio es una confusión ordinaria.

**Decisión, dos partes:**

1. **El montaje provisional queda anulado.** No se monta nada hasta tener
   componentes de valor conocido. El diseño válido es el de
   [ADR-013](#adr-013).
2. **Los valores de las resistencias se miden con el multímetro antes de
   usarlos.** El código de colores sirve para localizar una pieza en el cajón,
   no para decidir un divisor. Cualquier valor que entre en un cálculo de este
   proyecto tiene que venir de una medición, y el inventario de
   [HARDWARE.md](HARDWARE.md) marca cuáles lo están.

**Compra necesaria**, por orden de preferencia:

- **Un surtido de resistencias** (600–1000 piezas, 5–8 €). Vienen en bolsas
  etiquetadas con el valor impreso, así que el problema desaparece de raíz.
- **Lo mínimo:** dos de 15 kΩ y dos de 33 kΩ para ADR-013. O, aprovechando las
  9.1 kΩ que sí están medidas, **dos de 22 kΩ**: dejan el bus en 3.87 V y el
  GPIO en 2.74 V.

**Qué se pierde:** el proyecto vuelve a quedar a la espera de un pedido. Se
acepta: montar sobre valores supuestos es exactamente el modo de fallo que la
regla 5 de la [política](POLITICA_DOCUMENTACION.md) existe para evitar, y esta
vez el supuesto venía disfrazado de dato.

---

<a id="adr-016"></a>

## ADR-016 — La sonda final: 9.1 kΩ / 27 kΩ, sobre inventario medido

**Fecha:** 2026-08-02
**Estado:** **Reemplazada por [ADR-022](#adr-022)** — el pull-up real resultó ser
de 2.4 kΩ y no de 9.1 kΩ, así que este divisor deja el GPIO a 3.5 V, a un 2% del
máximo absoluto. Antes de eso, [ADR-018](#adr-018) ya había corregido dos datos
de abajo: la impedancia real es 10.9 kΩ y no 6.8 kΩ, y el procedimiento de
comprobación al conectar no es aplicable con un multímetro
**Reemplaza a:** [ADR-013](#adr-013), que sigue siendo correcto pero exige
componentes que no hay y no aprovecha los que sí

**Contexto:** Medido con multímetro el cajón entero —30 piezas— el inventario
real quedó fijado. Aparecen **dos resistencias de 9.1 kΩ** confirmadas, que son
un valor excelente para la parte de arriba del divisor, y desaparecen las de
15 kΩ que nunca existieron.

Con eso, la compra necesaria se reduce de cuatro resistencias a **dos**.

Sigue sin poder montarse solo con el inventario. Cada divisor necesita sumar
unos 25 kΩ para dejar el bus por encima de 3.7 V con el pull-up interno de
9.1 kΩ; dos divisores, 50 kΩ. Todo el material entre 1 kΩ y 10 kΩ suma 43 kΩ.
La de 74 kΩ permite armar una línea buena pero consume todo el material medio y
deja la segunda con 7 kΩ, que hunden el bus a 2.2 V.

**Decisión:** divisor de **9.1 kΩ arriba y 27 kΩ abajo** en cada línea.

| | Valor | Exigido |
|---|---|---|
| Carga sobre el bus | 36.1 kΩ | — |
| Nivel alto del bus | 3.99 V | ≥ 3.5 V |
| Tensión en el GPIO | 2.99 V | ≥ 2.5 V, ≤ 3.6 V |
| Impedancia vista por el GPIO | 6.8 kΩ | — |

Se eligió 27 kΩ entre las tres opciones válidas porque deja **las dos tensiones
centradas** en su rango, lejos de ambos límites. 22 kΩ deja el GPIO en 2.74 V,
más cerca del umbral; 33 kΩ lo deja en 3.23 V, más cerca del máximo.

Frente a ADR-013 (15 k / 33 k) gana además en impedancia: 6.8 kΩ en vez de
10.3 kΩ, así que **redondea menos los flancos** y tolera un bus más rápido.
Cede algo de margen en el nivel del bus: 3.99 V en vez de 4.20 V.

**Comprobación al conectar**, como en cualquier versión de la sonda: medir
rojo↔azul y verde↔azul antes y después. Debe pasar de ~5 V a ~4.0 V; por debajo
de 3.6 V, desconectar.

**Qué se pierde:** 0.2 V de margen sobre el umbral del chip respecto a ADR-013,
a cambio de flancos más limpios y de necesitar dos resistencias en vez de
cuatro. Se acepta: 0.49 V de margen sigue siendo holgado, y el pull-up real solo
puede ser más fuerte que el típico si la placa lleva los 10 kΩ externos del
circuito de aplicación, nunca más débil por ese motivo.

---

<a id="adr-017"></a>

## ADR-017 — El accionamiento va con relés de estado sólido, no con el módulo de relés mecánicos

**Fecha:** 2026-08-02
**Estado:** **Reemplazada en su medio por [ADR-021](#adr-021)** — el análisis
técnico de abajo sigue siendo correcto, pero su premisa de coste ("cuesta lo
mismo que un café") es falsa en Colombia, donde la pieza vale ~$120.000 COP
**Corrige el medio, no el fin, de:** [ADR-003](#adr-003) y [ADR-009](#adr-009),
cuyo razonamiento —contacto seco en paralelo al pulsador— sigue intacto

**Contexto:** El plan usaba el módulo de 4 relés Songle SRD-05VDC-SL-C que ya
está en el inventario. Son relés de potencia de 10 A.

Lo que van a conmutar no es potencia: es señal. El pulsador cierra una línea
DIG contra una KI, con **2 kΩ en serie** en la matriz de teclado (recomendación
del propio datasheet) y un pull-down interno en la KI de 30–90 µA. La corriente
que pasa por ese contacto es de microamperios.

Un contacto mecánico necesita una corriente mínima —del orden de decenas de
miliamperios— para atravesar la película de óxido y sulfuro que se forma sobre
la plata. Por debajo de ese umbral el contacto **se vuelve intermitente con el
tiempo**. El modo de fallo es el peor posible para este proyecto: funciona
perfectamente durante semanas y luego empieza a fallar una vez de cada diez, sin
patrón, y el síntoma apunta al firmware o al bus antes que al relé.

**Decisión:** el accionamiento se hace con **relés de estado sólido
optoacoplados** de salida MOSFET (photoMOS): AQY212GS, TLP222A, TLP175A o
equivalentes. Son contacto seco real, aislado y bidireccional, sin contactos
metálicos que oxidar. Se controlan desde un GPIO con una resistencia en serie
para el LED de entrada.

Hacen falta cuatro —subir, bajar, M1, M2— y conviene tener repuestos. El reset
sigue quedando físicamente fuera del circuito ([ADR-008](#adr-008)).

**Qué se pierde:** hay que comprarlos, y el módulo de relés que ya estaba en el
inventario queda sin uso en este proyecto. Cuesta unos pocos euros frente a un
fallo intermitente que aparecería meses después de dar el trabajo por terminado.

**Sin verificar:** que el módulo mecánico fallaría. Es un modo de fallo
documentado y ampliamente conocido en conmutación de señal, no una observación
propia. Se acepta como razón suficiente porque comprobarlo exigiría meses de
uso y el componente alternativo cuesta lo mismo que un café.

---

<a id="adr-018"></a>

## ADR-018 — Correcciones a la caracterización de la sonda: impedancia real y criterio de verificación

**Fecha:** 2026-08-03
**Estado:** Activa
**Corrige dos datos publicados en:** [ADR-013](#adr-013), [ADR-014](#adr-014) y
[ADR-016](#adr-016). **La decisión de fondo no cambia:** la sonda sigue siendo
el divisor de **9.1 kΩ / 27 kΩ** de ADR-016, sobre rojo y verde.

**Contexto:** revisando el proyecto entero antes de soldar aparecen dos errores
en cómo se describe y se comprueba la sonda. Ninguno cambia el circuito; los dos
cambian lo que hay que hacer en la mesa y lo que significa lo que se lea.

### Corrección 1 — la impedancia publicada solo vale para el flanco de bajada

ADR-016 da **6.8 kΩ** como impedancia vista por el GPIO, que es 9.1 ∥ 27. Ese
número es correcto **solo mientras el chip tira la línea abajo**: entonces la
parte de arriba del divisor cuelga de algo que está prácticamente a GND.

En el flanco de subida la fuente no es GND, es el **pull-up interno de 9.1 kΩ**,
que queda en serie con la resistencia de arriba:

```
(9.1 kΩ + 9.1 kΩ) ∥ 27 kΩ = 18.2 ∥ 27 = 10.9 kΩ
```

El flanco lento de un bus open-drain es siempre el de subida, así que **10.9 kΩ
es el número que manda**, no 6.8 kΩ. El mismo error afecta a los diseños
anteriores: ADR-013 declaraba 10.3 kΩ y su valor real es (15 + 9.1) ∥ 33 =
**13.9 kΩ**.

**Lo que no cambia:** la comparación entre diseños se mantiene en el mismo orden.
ADR-016 sigue siendo mejor que ADR-013 en este aspecto — 10.9 kΩ contra
13.9 kΩ — así que la razón por la que se eligió sigue siendo válida. Lo único
que cambia es el margen: es menor del que decía el texto.

**Consecuencia práctica:** el umbral de velocidad a partir del cual el divisor
deja de servir es más bajo de lo que sugería [firmware/README.md](../firmware/README.md).
No se puede dar una cifra exacta sin conocer la capacidad parásita real del
montaje, que depende de la longitud de los hilos y de la protoboard. **Se decide
por medición, no por cálculo:** el sniffer informa del periodo de reloj más corto
que ha visto, y si la captura sale sucia el remedio ya está declarado abajo.

**Plan B, que se re-ancla aquí porque quedó huérfano.** ADR-013 declaraba un plan
B —un **buffer no inversor alimentado a 3.3 V con entradas tolerantes a 5 V**,
tipo 74LVC2G17— y ADR-016 lo reemplazó sin volver a mencionarlo. Sigue vigente:
si la captura sale con bits perdidos, la solución **no** es bajar el valor de las
resistencias (eso carga el bus y devuelve al problema de [ADR-005](#adr-005)),
sino el buffer, que carga la línea con microamperios. Un adaptador de nivel con
BSS138 es la otra opción válida y **sustituye al divisor, no se suma a él**: su
resistencia de 10 kΩ va del bus a 5 V, o sea que refuerza el pull-up en vez de
cargar la línea contra GND.

### Corrección 2 — el nivel del bus se verifica por cociente, no por valor absoluto

ADR-014, ADR-016, [HARDWARE.md](HARDWARE.md), [SEGURIDAD.md](SEGURIDAD.md) y
[firmware/README.md](../firmware/README.md) repiten el mismo criterio: medir
rojo↔azul y verde↔azul, esperar que pasen de ~5 V a ~4.0 V, y desconectar si
baja de 3.6 V.

**Ese criterio no es aplicable con un multímetro.** Un multímetro en tensión
continua promedia, y el bus está conmutando: la caja refresca el display cada
~8 ms y sondea el teclado cada ~40 ms. Lo que se lee no es el nivel alto de
reposo, es el promedio entre el nivel alto y el bajo, pesado por el tiempo que la
línea pasa en cada uno.

Cuánto baja depende del ciclo de trabajo del bus, que **no se conoce hasta la
primera captura**. Puede ser casi imperceptible o puede ser mucho. Es decir: el
criterio publicado falla en una dirección desconocida. Si el bus está activo una
fracción apreciable del tiempo, las dos lecturas salen por debajo de lo
calculado, el número de después cae por debajo de 3.6 V, y el procedimiento
manda desconectar una sonda que está perfectamente bien. Parar el proyecto por
una falsa alarma es exactamente lo que este ADR evita.

**Decisión: el criterio es el cociente entre las dos lecturas.**

Antes de conectar la sonda el promedio vale `D · 5.0`; después vale `D · 3.99`.
El ciclo de trabajo `D` es el mismo en las dos y se cancela:

```
cociente = lectura_con_sonda / lectura_sin_sonda ≈ 0.80
```

Y como el nivel alto sin sonda es VDD = 5.0 V por definición, el cociente **es**
la atenuación del nivel alto. De ahí sale el nivel real del bus sin necesidad de
verlo:

| Cociente | Nivel alto real del bus | Qué hacer |
|---|---|---|
| **≥ 0.75** | ≥ 3.75 V | Correcto, seguir. Lo esperado es 0.80 |
| 0.70 – 0.75 | 3.50 – 3.75 V | Funciona, pero el margen es fino. Parar y pensar antes de seguir |
| **< 0.70** | **< 3.50 V** | **Desconectar.** Por debajo del VIH del chip |

**Condición de validez, que no es opcional:** las dos lecturas tienen que
tomarse con **el mismo tráfico en el bus**. Escritorio encendido y quieto,
display mostrando el mismo número, sin tocar ningún botón entre una medida y la
otra. Si en una de las dos el escritorio se movió, el ciclo de trabajo cambió,
`D` ya no se cancela y el cociente no significa nada.

### Lo que se gana de propina: el pull-up real, medido

El cociente da el valor del pull-up interno, que hasta ahora solo se tenía del
datasheet y como **típico**, sin mínimo garantizado ([ADR-013](#adr-013)):

```
Rpull-up = 36.1 kΩ · (1 - cociente) / cociente
```

Cociente 0.80 → 9.0 kΩ, que confirmaría el datasheet. Cociente 0.75 → 12.0 kΩ.
Cociente 0.70 → 15.5 kΩ, que es el pull-up más débil que este divisor tolera.
Dicho al revés: **el diseño aguanta un pull-up hasta un 70 % más débil que el
típico** antes de rozar el umbral. Ese margen no estaba cuantificado en ningún
sitio y es la respuesta a la duda que ADR-014 dejó abierta.

**Qué se pierde:** una medición más —la de antes de conectar la sonda, que ya
estaba en el plan— y la comodidad de un umbral absoluto que se pudiera leer de
un vistazo. A cambio, la comprobación deja de depender de un dato que no
tenemos, y de paso mide lo único que quedaba supuesto en el diseño.

---

<a id="adr-019"></a>

## ADR-019 — La sonda nunca queda conectada al bus con el ESP32 sin alimentar

**Fecha:** 2026-08-03
**Estado:** Activa
**Amplía:** [ADR-007](#adr-007), que fija la alimentación por USB pero no dice
nada del orden de conexión

**Contexto:** revisando qué puede salir mal en la fase 2 aparece un estado que
no estaba contemplado: **la sonda conectada al bus con el escritorio encendido y
el ESP32 sin alimentar.** Es un estado normal en la mesa — se desenchufa el USB
para mover el portátil, o se deja el montaje a medias entre sesiones.

Todo GPIO lleva un diodo de protección hacia su propia alimentación. Con el
ESP32 sin alimentar, ese raíl está a 0 V, y el diodo conduce en cuanto el nodo
del divisor sube por encima de ~0.7 V. El nodo queda enganchado ahí, y eso
arrastra el bus hacia abajo:

```
5 V ──[9.1k pull-up]──┬──[9.1k]──┬── GPIO enganchado a ~0.7 V
                      │          └──[27k]── GND
                   bus = 2.85 V
```

**2.85 V, muy por debajo del VIH de 3.5 V que el chip necesita para leer un
"1".** El mando dejaría de funcionar correctamente mientras esté en ese estado.

Dos precisiones, porque la diferencia importa:

- **No hay daño.** La corriente que entra por el diodo es de unos **210 µA**,
  limitada por los 9.1 kΩ de arriba. Está órdenes de magnitud por debajo de lo
  que estropea un pin. El chip tampoco sufre: solo ve una carga mayor.
- **El número exacto no es predecible.** El cálculo de 2.85 V supone el raíl del
  ESP32 firmemente a 0 V. En la práctica esos 210 µA lo van cargando y el diodo
  deja de conducir en algún punto que depende de la fuga de la placa. Puede
  quedar en cualquier sitio entre 2.85 V y lo normal. Esa impredecibilidad es
  justo la razón para prohibir el estado en vez de caracterizarlo.

**Por qué merece un ADR y no una nota:** el síntoma —el mando falla, o parpadea,
o no responde— aparecería justo después de haber soldado en la placa del mando.
La conclusión natural sería "rompí algo al soldar", que es irreversible en la
cabeza aunque no lo sea en el hardware. Es el mismo patrón que
[ADR-005](#adr-005) evitó: un fallo cuyo síntoma apunta al sitio equivocado.

**Decisión: orden de conexión obligatorio, en los dos sentidos.**

1. Para conectar: **primero el USB**, se espera a que el ESP32 arranque, y
   **después** se conectan los hilos rojo y verde a la entrada del divisor.
2. Para desconectar: **primero los hilos** del bus, **después** el USB.
3. Entre sesiones el montaje no se deja a medias: o el ESP32 alimentado, o los
   hilos del bus fuera del divisor. La forma cómoda de cumplirlo es dejar rojo y
   verde en jumpers que se sacan de la protoboard.

**El riesgo solo existe con el ESP32 sin alimentar, no durante su arranque.** Con
el raíl a 3.3 V el diodo no conduce hasta que el nodo pasa de ~4.0 V, y el nodo
nunca pasa de 2.99 V por diseño. Un reinicio del ESP32 con todo conectado es
inocuo.

**Alternativa descartada:** proteger por hardware, con un buffer o con una
resistencia en serie mayor. Se descarta para la fase 2 porque el problema es de
procedimiento y el procedimiento es trivial, mientras que cualquier hardware
extra añade componentes que no hay y una fuente más de error de montaje. Si en
fase 3 el montaje pasa a ser permanente y alimentado por su propio cargador, la
decisión se revisa: allí el estado "escritorio encendido, ESP32 sin corriente"
deja de ser un descuido y pasa a ser lo que ocurre en cada apagón.

**Qué se pierde:** un orden que hay que recordar, y la comodidad de desenchufar
el USB sin pensar. Barato frente a diagnosticar un mando "roto" que no lo está.

---

<a id="adr-020"></a>

## ADR-020 — CLK se mueve a GPIO18: el pin deja de depender de qué módulo sea

**Fecha:** 2026-08-03
**Estado:** Activa — **P18 confirmado presente en la bornera** el 2026-08-03 por
fotografía del producto, que cierra la única comprobación que este ADR dejaba
pendiente. Mapa completo en [HARDWARE.md](HARDWARE.md)
**Cierra el supuesto sin verificar que anotó:** [ADR-018](#adr-018) (el módulo
WROOM/WROVER), eliminándolo en vez de resolviéndolo

**Contexto:** CLK estaba asignado a **GPIO16**. En los módulos **WROVER**, los
GPIO 16 y 17 están cableados a la PSRAM y no se pueden usar. Cuál de los dos
módulos lleva esta placa no está verificado:

- El blindaje metálico **no es legible** — o no lleva serigrafía, cosa normal en
  módulos de clon.
- La única evidencia es la **descripción del vendedor: `ESP-WROOM-32`**, que es
  el nombre antiguo de Espressif para el ESP32-WROOM-32. Apunta a WROOM, pero es
  una ficha de tienda, no una observación.

Este proyecto ya ha pagado tres veces por tratar un dato de segunda mano como si
fuera medido: el `ROJO = VCC probable` del handover, el pinout de components101,
y tres resistencias leídas por color. La ficha del vendedor es exactamente la
misma clase de dato.

Hay dos salidas. Una es **verificar**: además del blindaje, el tamaño del módulo
lo dice sin leer nada, porque son distintos —**WROOM 18 × 25.5 mm, WROVER
18 × 31.4 mm**, seis milímetros más largo, inconfundible con una regla. La otra
es **quitar la dependencia**.

**Decisión: CLK pasa de GPIO16 a GPIO18.** DIO se queda en GPIO4.

GPIO18 cumple todo lo que hay que cumplir, y en los dos módulos:

| Requisito | GPIO18 |
|---|---|
| Libre en WROOM **y** en WROVER | Sí — la PSRAM solo usa 16 y 17 |
| No es pin de arranque (0, 2, 5, 12, 15) | No lo es |
| No es de la flash (6–11) | No lo es |
| Por debajo de 32, que es lo que exige el `static_assert` del sketch | Sí |

Con esto la pregunta "¿WROOM o WROVER?" **deja de tener consecuencias** en el
proyecto. Sigue mereciendo la pena anotar la respuesta en
[HARDWARE.md](HARDWARE.md) si algún día se lee, pero ya no bloquea nada ni puede
producir un "CLK muerto" con el cableado correcto.

**Se decide ahora y no después porque ahora es gratis:** no hay nada soldado ni
cableado. Cambiar de pin cuesta una constante en el sketch. Después de montar,
costaría desmontar.

**Qué se pierde:**

- Hay que **confirmar a ojo que P18 está en la bornera** de la placa de
  expansión antes de cablear. En los DevKit de 38 pines P18 y P19 van en la
  misma tira que P16 y P17, unas posiciones más abajo, pero eso no está
  verificado en esta placa concreta. Si no estuviera accesible, **GPIO19, 21, 22,
  23, 25, 26 o 27** valen igual; lo que no vale es volver a 16 o 17.
- El plano viejo [hardware/plano_divisores_v1.svg](hardware/plano_divisores_v1.svg)
  queda aún más desactualizado. Ya estaba marcado como "no debe montarse" por
  [ADR-005](#adr-005).

**Efecto lateral bueno:** el ESP32 de repuesto de [COMPRAS.md](COMPRAS.md) ya no
tiene que ser obligatoriamente WROOM. Cualquiera de los dos sirve, lo que hace
la compra más fácil y más barata.

---

<a id="adr-021"></a>

## ADR-021 — El accionamiento se hace con lo barato, y el riesgo se acepta en vez de pagarlo

**Fecha:** 2026-08-03
**Estado:** Activa
**Reemplaza el medio elegido en:** [ADR-017](#adr-017), cuyo análisis técnico
sigue siendo correcto pero cuya **premisa de coste es falsa en este mercado**

**Contexto — la evidencia nueva.** ADR-017 eligió relés de estado sólido
(photoMOS) y se justificó con una frase textual: *"el componente alternativo
cuesta lo mismo que un café"*. En Colombia, un **G3VM-61A1 cuesta ~$120.000
COP**. Cuatro, **~$480.000** — más que todo el resto del proyecto junto, que
ronda los $220.000.

Eso no es una opinión distinta, que la política no admite como motivo para
reabrir: es **la premisa explícita del ADR, falsada con un precio real**.

**Y hay un análisis que ADR-017 no hizo: qué pasa exactamente cuando falla.**

La conmutación en seco degrada el contacto por óxido, y el resultado es que **el
relé no cierra**. Traducido: se pulsa el botón y el escritorio no se mueve.
Molesto, y ya.

El fallo **peligroso** —el contacto pegado, que dejaría el escritorio subiendo
sin parar y que motiva media [SEGURIDAD.md](SEGURIDAD.md)— lo causan corrientes
altas y el arco eléctrico al abrir. **A 90 µA no hay arco posible.** A esta
corriente, un relé mecánico prácticamente no puede fallar en el modo que
importa.

Es decir: el riesgo real de usar los relés que ya están en el cajón es **una
molestia dentro de unos meses, no un peligro**. ADR-017 escribió el modo de fallo
como algo grave sin distinguir estas dos cosas.

**Decisión: por orden de preferencia, y ninguna de las tres cuesta $480.000.**

1. **PC817**, optoacoplador corriente, unos miles de pesos. Pendiente de las dos
   pruebas de multímetro y de confirmar con la captura que la corriente por el
   botón va siempre en el mismo sentido. Detalle en [COMPRAS.md](COMPRAS.md).
2. **Los relés mecánicos que ya hay en el inventario**, si el PC817 no encaja.
   Cuesta **cero**. Se acepta que puedan volverse intermitentes con el tiempo;
   si ocurre, se sustituyen entonces, con todo lo demás ya funcionando y
   sabiendo exactamente dónde mirar.
3. **Relé reed**, si aparece barato en tienda. Va sellado herméticamente, que es
   la solución clásica a este problema, y es mucho más barato que un photoMOS.
   Sin verificar precio ni disponibilidad local.
4. **photoMOS solo si los tres anteriores fallan** y el problema se vuelve real y
   medido, no teórico.

**Qué se pierde:** la garantía de fiabilidad a años vista que daba el photoMOS.
Se acepta por tres razones: **esto es un proyecto para pasar el rato**, el fallo
previsible es benigno y reversible, y gastar $480.000 en una pieza para evitar
una molestia futura en un proyecto de $220.000 no es proporcionado.

---

<a id="adr-022"></a>

## ADR-022 — El divisor pasa a 16.3 kΩ / 27 kΩ: el pull-up real es 2.4 kΩ, no 9.1 kΩ

**Fecha:** 2026-08-06
**Estado:** Activa
**Reemplaza a:** [ADR-016](#adr-016), cuyo cálculo era correcto para el pull-up
que se suponía, pero no para el que hay
**Corrige un razonamiento de:** [ADR-013](#adr-013)

**Contexto — la primera medición del bus con el escritorio encendido.** Hasta hoy
el valor del pull-up venía del datasheet: 550 µA típicos, unos 9.1 kΩ a 5 V. Con
la sonda montada y el escritorio en marcha, el cociente de
[ADR-018](#adr-018) lo mide por fin:

| | Medido |
|---|---|
| Bus sin sonda | 4.9 V |
| Bus con sonda (9.1 k / 27 k) | 4.6 V |
| Cociente | **0.94** |

```
R_pullup = 36.1 kΩ × (1 − 0.94) / 0.94 ≈ 2.4 kΩ
```

**El pull-up real es casi cuatro veces más fuerte que el del datasheet.** El
mando lleva resistencias de pull-up externas que no estaban documentadas en
ninguna parte, además de las internas del chip.

**El error de razonamiento que esto destapa.** ADR-013 contempló esta
posibilidad y la despachó así: *"si esta placa las lleva, el pull-up resultante
baja a ~4.8 kΩ, o sea más fuerte. Dimensionar para 9.1 kΩ es seguro exista lo que
exista fuera del chip."*

**Eso es falso, y la mitad que falta es la peligrosa.** Un pull-up más fuerte
juega a favor del **bus** —se hunde menos, más margen sobre el VIH del chip— pero
juega **en contra del GPIO**: el divisor atenúa menos y al pin del ESP32 le llega
más tensión. ADR-013 solo miró un lado de la desigualdad.

**Consecuencia medida:** con el divisor de ADR-016, el nodo del GPIO quedó en
**3.5 V de promedio**. Como el bus está en reposo alto el ~94% del tiempo, el
pico real ronda **3.53 V**, contra un **máximo absoluto de 3.6 V** en los GPIO
del ESP32. Margen del 2%, cuando el diseño preveía el 17%.

**Decisión: añadir una resistencia de 7.4 kΩ en serie con la de 9.1 kΩ de cada
línea**, dejando la parte de arriba en **16.3 kΩ** y la de abajo en los 27 kΩ que
ya estaban.

| | ADR-016 | **ADR-022** | Exigido |
|---|---|---|---|
| Arriba | 8.9 kΩ | **16.3 kΩ** | — |
| Abajo | 27 kΩ | 27 kΩ | — |
| Nivel del bus | 4.6 V | **4.7 V** | ≥ 3.5 V |
| **Tensión en el GPIO** | **3.5 V** ⚠️ | **2.9 V** ✅ | ≥ 2.5 V y ≤ 3.6 V |
| Impedancia, flanco de subida | 10.8 kΩ | 11.0 kΩ | — |

**Los 2.9 V son medidos, no calculados** — el valor previsto era 2.95 V y el
multímetro dio 2.9 V, lo que además confirma por segunda vía la estimación de
2.4 kΩ del pull-up.

Cargar más el bus sale gratis aquí precisamente por lo mismo que causó el
problema: con un pull-up de 2.4 kΩ, subir la carga de 36 a 43 kΩ apenas mueve el
nivel del bus. **El pull-up fuerte da margen para corregir el propio problema que
crea.**

**Qué se pierde:** dos resistencias más del inventario —las dos 7.4 kΩ, que
quedan usadas— y una vuelta de montaje con el escritorio ya encendido. La
impedancia sube de 10.8 a 11.0 kΩ, diferencia despreciable.

**Lo que no cambia de ADR-016:** el criterio de verificación por cociente de
ADR-018 sigue siendo el correcto, y de hecho es lo que detectó esto. Sin esa
medición, el montaje habría quedado funcionando con un 2% de margen sobre un
máximo absoluto, sin que nadie lo supiera.

---

<a id="adr-023"></a>

## ADR-023 — Los cuatro canales se acotan con el mismo limitador de pulso de 300 ms

**Fecha:** 2026-08-06
**Estado:** Activa — el **requisito** sigue en pie; el **medio** lo fija
[ADR-024](#adr-024): lo implementa el watchdog del ESP32, y el monoestable queda
aplazado
**Cierra la decisión pendiente de:** [SEGURIDAD.md](SEGURIDAD.md), que bloqueaba
abrir la fase 3
**Amplía:** [ADR-010](#adr-010), que exigía acotar por hardware solo M1 y M2
**Corrige el mecanismo descrito en:** [ADR-009](#adr-009)

**Contexto.** Quedaba pendiente decidir si subir y bajar se acotan por hardware,
como M1 y M2. El razonamiento de partida era: un contacto pegado con el canal de
subir activo mueve el escritorio hasta el tope, y todas las protecciones para ese
caso viven en el firmware, que es lo que se supone que ha fallado.

### Lo que la medición cambió, y era casi todo

**Primer hallazgo, que invalidó el planteamiento inicial.** Subir y bajar tienen
**dos regímenes distintos**, no uno:

| | Qué hace | ¿Se para solo? |
|---|---|---|
| **Toque corto** | Mueve ~1 cm | **Sí** |
| **Mantener y soltar** | Arranca movimiento continuo | **No.** Sigue hasta que se pulse cualquier botón |

Medido en [2026-08-06-pulsadores.log](capturas/2026-08-06-pulsadores.log): tras
soltar, el escritorio siguió subiendo **5 cm en 5 s** y, en otra prueba,
bajando **6 cm en 6,6 s**. En los dos casos paró solo cuando se pulsó otra tecla.

**Consecuencia: un circuito que ABRA el canal no sirve de nada.** El movimiento
continuo ya está en marcha e ignora el estado del contacto. Para parar hay que
**pulsar**, o sea *cerrar* un contacto — y un vigilante que actúa por ausencia de
señal no puede hacer eso.

*Esto descartó de golpe las dos primeras propuestas que se barajaron —un latido
que abriera el canal al dejar de recibir señal, y un one-shot que cortara el
cierre a los N segundos—. Las dos protegían contra un modo de fallo que no es el
que existe.*

**Segundo hallazgo, que devolvió la solución.** El umbral que separa toque de
movimiento continuo está en **2,2–2,6 s**, medido en
[2026-08-06-umbral-toque-vs-continuo.log](capturas/2026-08-06-umbral-toque-vs-continuo.log):

| Duración de la pulsación | Resultado |
|---|---|
| 200 ms · 1,0 s · 1,6 s · 1,8 s | Toque, se para solo |
| **2,2 s** | Toque — la más larga que se paró sola |
| **2,6 s** | **Continuo** — la más corta que se disparó |
| 2,8 s | Continuo |

Y el umbral de grabar preset con M1/M2 es **3,0 s** ([HARDWARE.md](HARDWARE.md)).
**Los dos rondan los 2–3 segundos**: la caja parece tener un único concepto de
"pulsación larga" y aplicarlo igual a los cuatro botones.

**Decisión: un limitador de ancho de pulso por hardware a 300 ms, idéntico en los
cuatro canales.** El firmware solo emite toques; el hardware garantiza que
ninguno pueda durar más, pase lo que pase con el GPIO o con el contacto.

```
160 ms          300 ms                      2200 ms
 |---------------|---------------------------|
 mínimo       elegido                    empieza
 del chip                                el peligro
```

- **Por arriba:** 7 veces por debajo del umbral más bajo medido.
- **Por abajo:** casi el doble de los 160 ms que el chip exige para ver la
  pulsación (dos periodos de escaneo de 80 ms) [datasheet].

**Con eso, los dos modos de fallo peligrosos desaparecen a la vez:**

- Un canal de subir o bajar pegado **no puede arrancar movimiento continuo**.
  Como mucho mueve un centímetro.
- Un canal de memoria pegado **no puede sobrescribir el preset**. Como mucho lo
  recupera.

Un solo mecanismo, replicado cuatro veces, en vez de dos protecciones distintas.

**Refuerzo secundario, no protección principal.** El display **parpadea cuando
arranca un movimiento continuo** — observado a ojo y confirmado en el bus: 0
parpadeos tras el toque de 2,2 s, 2 y 3 tras los continuos. El firmware puede
detectar por ahí que se disparó un movimiento que no pidió, y mandar un toque
para pararlo. Es una red por software, y por tanto **no cuenta como la
protección**; se anota porque sale gratis.

**Qué se pierde.**

- **Los movimientos largos van a trozos.** Para una altura arbitraria hay que
  encadenar toques de ~1 cm, lo que hace un recorrido largo unas dos veces más
  lento que un movimiento continuo. En la práctica pesa poco: el uso real es
  sentado ↔ de pie, que son dos toques de memoria, y el ajuste fino son dos o
  tres toques.
- **El movimiento continuo queda fuera del alcance del ESP32**, para siempre y a
  propósito. Es el único modo capaz de llevar el escritorio hasta el tope sin
  que nada lo pare.
- Cuatro limitadores en vez de dos, aunque son el mismo circuito repetido.

**Corrección a [ADR-009](#adr-009).** Aquel ADR describe el lazo cerrado como
*"mantener subir, vigilar la altura en el display, soltar al llegar"*. **Soltar
no para el escritorio**, así que ese mecanismo, tal como está escrito, no
funciona. La decisión de fondo de ADR-009 —que el contacto seco en lazo cerrado
alcanza cualquier altura— **sigue siendo cierta**, pero por el camino de encadenar
toques, no por el de mantener y soltar. Los hechos correctos ya estaban en la
tabla de contexto del propio ADR-009 y en [HARDWARE.md](HARDWARE.md); lo que
estaba mal era la frase que describía el mecanismo.

---

<a id="adr-024"></a>

## ADR-024 — El limitador de pulso lo hace el watchdog del ESP32, y el monoestable queda para después

**Fecha:** 2026-08-06
**Estado:** Activa
**Cambia el medio, no el fin, de:** [ADR-023](#adr-023), cuyo requisito —que
ningún pulso pueda durar más de unos cientos de ms aunque el firmware muera—
sigue intacto

**Contexto.** ADR-023 fijó un limitador de ancho de pulso por hardware en los
cuatro canales, sin decir con qué se implementa. Construirlo con un monoestable
exige comprar integrados y condensadores que no hay, y en Bogotá eso son **días
de espera**.

**Dos cosas que ADR-023 no pesó:**

1. **El accionamiento es de estado sólido.** Con PC817 no hay contacto metálico
   que se suelde ni que se quede pegado, que era buena parte de lo que motivaba
   la preocupación. Un fototransistor en cortocircuito es una avería de
   componente, mucho más rara que un contacto pegado.
2. **El ESP32 lleva un watchdog por hardware, y no se consideró.** Es un
   temporizador que reinicia el chip si el firmware deja de refrescarlo. **Actúa
   precisamente cuando el CPU deja de ejecutar**, que es el fallo del que nos
   protegemos — no depende de que el programa haga nada, sino de que deje de
   hacerlo.

**Decisión: el watchdog configurado a 1 segundo es la protección de la fase 3.**

```
el firmware se cuelga
   |  <= 1 s
el watchdog reinicia el chip
   |
los GPIO quedan en alta impedancia
   |
el LED del optoacoplador se apaga
   |
el canal se abre
```

**Un segundo contra los 2.2 s que tarda en dispararse un movimiento continuo**, y
contra los 3.0 s de grabar un preset. Más del doble de margen, y coste cero.

⚠️ **Condición imprescindible antes de cablear nada, y hereda de
[ADR-010](#adr-010) punto 4: verificar el nivel de los GPIO de control durante
el arranque y el reset.** Toda esta protección se apoya en que un reinicio deje
los pines sin conducir. **Si alguno se pone alto durante el arranque, el
watchdog no abriría el canal: lo activaría.** Se mide con el multímetro, con el
ESP32 arrancando y sin nada conectado, antes de soldar al mando.

**Qué se pierde, y es real:** **no hay segunda barrera.** Si el ESP32 fallara de
una forma que el watchdog no detecte, nada más abriría el canal. Un monoestable
independiente sí lo haría, porque no comparte nada con el ESP32.

Se acepta por proporción: estado sólido sin contactos que pegar, más del doble de
margen temporal, y un proyecto para pasar el rato que puede empezar mañana en
vez de dentro de una semana.

**El monoestable no se descarta: se aplaza.** Queda en la lista acumulada de
[COMPRAS.md](COMPRAS.md) como mejora declarada. **Condiciones que obligarían a
montarlo:**

- Que el sistema pase a funcionar **sin nadie delante** de forma habitual.
- Que se observe **un solo caso** de watchdog que no dispara.
- Que se cablee algo con contacto mecánico en vez de PC817.

**Lo que no cambia de ADR-017:** su análisis técnico. Un relé de potencia de 10 A
sigue siendo una mala elección para conmutar microamperios, y sigue siendo
verdad que puede volverse intermitente. Lo que cambia es qué se hace con ese
hecho: antes se pagaba por eliminarlo, ahora se acepta y se vigila.

**Regla que queda para el futuro:** los ADR de este proyecto se escribieron con
precios europeos. **Cualquier decisión que se justifique por coste hay que
revalidarla con precios de Bogotá antes de darla por buena.**

---

## ADR-025 — El puerto serie baja a 460800: a 921600 el ESP32 no recibe comandos

**Fecha:** 2026-08-21
**Estado:** Aceptado

### Contexto

`desk_sniffer` dejó de ser solo un volcador: desde el 2026-08-20 acepta los
comandos `1`–`4` para disparar los canales de accionamiento, y ese es el
mecanismo con el que se comprueba que un canal quedó soldado al pulsador
correcto. Sin comandos, no hay comprobación.

Al intentar la primera comprobación real, **ningún comando llegaba al sketch**.
La lectura funcionaba perfectamente — 24 KB de tráfico legible y bien
decodificado — pero el firmware no reaccionaba a nada de lo enviado.

Se descartaron las causas fáciles, en este orden:

1. **Puerto ocupado.** Lo estaba, por el Serial Monitor del Arduino IDE
   (`lsof` lo señaló: proceso `serial-monitor`). Se cerró el IDE. **No era eso**
   — el fallo siguió igual con el puerto libre.
2. **La herramienta de escritura.** Se probó con tres distintas: redirección de
   shell a un descriptor bidireccional, `arduino-cli monitor`, y un script de
   Python con `IOSSIOSPEED`, el ioctl nativo de macOS. **Las tres fallaron
   igual.**
3. **El firmware.** Se revisó `setup()`, `loop()` y `captureBurst()`.
   `Serial.available()` se consulta en cada vuelta del loop, las interrupciones
   solo se apagan durante los 2 ms de la ráfaga, y nada toca GPIO3 (el RX).
   **Nada que explicara el fallo.**
4. **El cable y la dirección Mac → placa.** Descartado por la propia subida de
   firmware: escribió a **898.8 kbit/s**, o sea que el Mac transmite a 921600 sin
   problema y el RX físico de la placa funciona.
5. **Inundación.** 63 bytes seguidos, uno cada 50 ms: **cero respuestas**.

Quedaba una sola variable sin probar: la velocidad.

### Qué se midió

Se compiló el mismo sketch, sin ningún otro cambio, a distintas velocidades:

| Velocidad | Lectura del bus | ¿Recibe comandos? |
|---|---|---|
| 921600 | Correcta, ~10 KB en 7 s | **NO** — 63 intentos, 0 respuestas |
| 460800 | Correcta, ~10 KB en 7 s | **SÍ** — 10 de 10 |
| 230400 | Correcta | **SÍ** |
| 115200 | Correcta | **SÍ** |

**Verificado.** La asimetría es real y reproducible: a 921600 la placa
**transmite** bien y **no recibe**. Es un límite del adaptador USB-serie, no del
ESP32 ni del sketch.

### Decisión

**`SERIAL_BAUD` pasa de 921600 a 460800.**

Es la velocidad más alta de las que sí reciben, verificada con 10 comandos de 10
respondidos después de descontar el banner de arranque.

### Qué se pierde

**La mitad del caudal del puerto.** Era el argumento con el que se eligió
921600, escrito en [../firmware/README.md](../firmware/README.md): a 115200 el
puerto sería el cuello de botella.

Ese argumento **sigue siendo válido, pero 460800 lo respeta**. El tráfico en
reposo son ~25 líneas por segundo de unos 45 caracteres, algo más de 1 KB/s,
contra los 46 KB/s que da 460800 — sobra un factor de cuarenta. El margen
perdido importa solo en el volcado crudo (`r`), que es el modo que más escupe.

**Qué comprobación lo confirmaría:** capturar con volcado crudo activado durante
un movimiento completo del escritorio y mirar si el contador de descartes sube.
No se ha hecho.

**Lo que no se pierde:** la captura del bus no depende del puerto. El muestreo
es por ráfagas a 4 MHz contra memoria, y el serie solo se toca entre ráfagas —
por diseño, según el comentario del propio `loop()`.

### Consecuencia práctica

Todos los procedimientos documentados que decían `921600` estaban bien cuando se
escribieron y **ahora son incorrectos**. Corregidos a 460800 en
[PLAN.md](PLAN.md), [capturas/README.md](capturas/README.md) y
[../firmware/README.md](../firmware/README.md).

---

## ADR-026 — El fallo de recepción del puerto serie no está explicado, y el ADR-025 no se reproduce

**Fecha:** 2026-08-21
**Estado:** Aceptado
**Corrige parcialmente:** [ADR-025](#adr-025--el-puerto-serie-baja-a-460800-a-921600-el-esp32-no-recibe-comandos), que no se toca

### Contexto

El ADR-025, escrito unas horas antes, concluyó que el problema era la velocidad
del puerto y que **a 460800 el sketch recibe 10 comandos de 10**. Sobre esa base
se cambió `SERIAL_BAUD` y se dio por desbloqueada la comprobación de los canales.

Al usar ese mecanismo para comprobar el canal 2, **no llegó ni un comando**.

### Qué se midió

Todo con el escritorio encendido y el mando en reposo. **Verificado:**

| Prueba | ¿Recibe? |
|---|---|
| `desk_sniffer` a 460800, con `serial_talk.py` | **NO** |
| `desk_sniffer` a 460800, con `arduino-cli monitor` (herramienta independiente) | **NO** |
| `desk_sniffer` a 115200, con `serial_talk.py` | **NO** |
| Ídem, con el `switch` instrumentado para delatar **cualquier** byte | **NO** — 0 bytes; `Serial.available()` era 0 |
| Sketch mínimo de eco, 115200 | **SÍ** |
| Eco + `setTxBufferSize(4096)` | **SÍ** |
| Eco + ráfaga de 8000 muestras con `noInterrupts()` | **SÍ** |
| Eco + bucle apretado que nunca cede CPU a FreeRTOS | **SÍ** |
| Eco + ráfaga + 5 líneas cada 35 ms (143 líneas/s) | **NO** |
| `desk_sniffer` con la captura desactivada | **SÍ** |
| `desk_sniffer` con `captureBurst` activo y `decodeBurst` desactivado | **SÍ** |
| `desk_sniffer` completo otra vez | **SÍ, 5 de 5** |

El upload escribe a 898.8 kbit/s, así que **el RX físico de la placa funciona**:
el bootloader recibe sin problema.

### El problema

**La última fila es funcionalmente idéntica al sketch que fallaba.** La única
diferencia son unas llaves alrededor de un bloque, que no cambian nada. Después
de eso el puerto recibió en 5 pruebas de 5, y en los 3 disparos posteriores del
canal 2.

Se descartan como causa única, **por medición**: la velocidad,
`setTxBufferSize`, el `noInterrupts()` de la ráfaga, el bucle que no cede CPU, y
el adaptador USB-serie.

Queda **una sola reproducción positiva del fallo**: caudal alto de impresión,
143 líneas por segundo. Pero el caudal real en reposo son **28 líneas/s, 1.4 KB/s
de los 11.5 KB/s que da 115200** — un 12%. **Supuesto, no verificado:** que el
mecanismo tenga que ver con la escritura por el puerto. No explica el caso real.

**La causa no está identificada.** Escribirlo como resuelto sería repetir
exactamente lo que hizo falsar al ADR-025.

### Decisión

1. **`SERIAL_BAUD` queda en 115200.** Es la única velocidad en la que hoy se ha
   comprobado de punta a punta —recepción de comandos y lectura del bus— en la
   misma sesión. El margen sobra en reposo; **con el volcado crudo (`r`) activo
   hay que vigilarlo**, que es el modo que más escupe.
2. ⚠️ **Antes de cualquier prueba de canal, mandar `h` y comprobar que el sketch
   responde la ayuda.** Sin ese paso, un fallo del puerto se atribuye al canal.
   **Ya ha pasado dos veces**: la captura
   [capturas/2026-08-21-canal2-disparo-bajar.log](capturas/2026-08-21-canal2-disparo-bajar.log)
   parecía un canal muerto y era el puerto.
3. **La línea de depuración que imprime cada byte recibido se queda** en
   `desk_sniffer`. Cuesta 64 bytes de programa y distingue "no llega" de "llega
   corrupto" sin volver a bisecar nada.
4. **El fenómeno queda abierto.** Si reaparece, lo primero es repetir la tabla de
   arriba en vez de suponer.

### Qué se pierde

La mitad del caudal frente a 460800. Aceptable por lo dicho en el punto 1, y
reversible en cuanto se entienda el fallo.

---

## ADR-027 — El ancho de pulso sube de 300 a 800 ms: con 300 el escritorio no se mueve

**Fecha:** 2026-08-21
**Estado:** Aceptado
**Corrige a:** [ADR-023](DECISIONS.md), que no se toca

### Contexto

El ADR-023 fijó el pulso en **300 ms** razonando sobre dos umbrales medidos: el
mínimo de 160 ms para que el chip vea la pulsación, y los 2.2 s que arrancan
movimiento continuo. 300 ms quedaba cómodamente en medio.

**El razonamiento tenía un supuesto que nadie comprobó:** que un pulso que el
chip *ve* es un pulso que *mueve el escritorio*. Son cosas distintas.

### Qué se midió

Con el canal 2 ya verificado y respondiendo `0x57`, se intentó bajar de 84 a
80 cm en lazo cerrado, leyendo la altura del bus entre pulso y pulso:

| Ancho | Pulsos | Movimiento |
|---|---|---|
| **300 ms** | 7 | **0 cm** — la altura no se movió de 84 |
| **800 ms** | 12 | **4 cm** — 84 → 80, cerca de 1 cm cada 3 pulsos |

**Verificado.** La caja registra la tecla con 300 ms —el `0x57` aparece en el
bus— pero el motor no llega a arrancar, o lo hace por menos del centímetro que
muestra el display.

Encaja con lo que ya había en [HARDWARE.md](HARDWARE.md): los toques que se
cronometraron el 2026-08-06 y que "mueven ~1 cm" eran de **1.0 a 2.2 s**. El más
corto que se probó entonces fue de 200 ms, y **no se anotó cuánto movió**.

### Decisión

**`PULSE_MS` pasa de 300 a 800 ms.**

Sigue **2.75× por debajo** de los 2.2 s que arrancan movimiento continuo, y
3.75× por debajo de los 3.0 s que graban un preset. El margen baja de 7× a 2.75×,
y a cambio el accionamiento **hace algo**, que es el requisito que faltaba.

### Qué se pierde

**Margen frente al movimiento continuo.** Con 300 ms, un contacto pegado movía
como mucho un centímetro; con 800 ms mueve algo más, pero **sigue sin poder
arrancar movimiento continuo ni sobrescribir un preset**, que es la propiedad que
sostiene [ADR-023](DECISIONS.md) y que aquí se conserva intacta.

---

## ADR-028 — Se admite el movimiento continuo, porque el mismo canal lo frena

**Fecha:** 2026-08-21
**Estado:** Aceptado
**Reabre:** [ADR-023](DECISIONS.md) y la regla de [SEGURIDAD.md](SEGURIDAD.md)
de "evitar que el movimiento continuo arranque". Ninguno se toca.

### Contexto

El diseño evitaba el movimiento continuo por una razón concreta y medida:
**soltar el contacto no lo para** —el escritorio siguió 6 cm en 6.6 s bajando— y
**para detenerlo hay que cerrar un contacto, no abrirlo**. El watchdog de
[ADR-024](DECISIONS.md) abre el canal, que es justo la acción que **no** sirve.

Sobre esa base se eligió mover a base de toques. Con [ADR-027](DECISIONS.md) eso
funciona, pero es lento: **12 pulsos y medio minuto para 4 cm**.

### La evidencia nueva

La medición del 2026-08-06 dejó una ambigüedad que nadie había visto: la tabla
decía *"sigue hasta que se pulse **cualquier** botón"*, y el texto de la propia
medición decía *"solo paró al pulsar **otra** tecla"*. **Si hiciera falta un
botón distinto, un solo canal cableado no podría frenar nunca.**

Se midió el 2026-08-21, bajando —a propósito: si el freno fallaba, el tope
físico de 73 cm detiene el escritorio; hacia arriba esta prueba no sería
segura—:

| Momento | Altura |
|---|---|
| Pulso largo de 2.8 s, durante | 82 → 79 cm |
| **Tras soltar el contacto** | **siguió solo**: 78, 77 cm |
| **Toque corto en el MISMO canal** | **paró en 76 cm**, y 8 s sin moverse |

**Verificado: el mismo botón frena su propio movimiento continuo.** La tabla
tenía razón y el texto era el impreciso.

Y la diferencia de velocidad es grande: **6 cm en 7 s** contra 12 pulsos.

### Decisión

**Se admite el movimiento continuo como modo de operación**, con estas
condiciones:

1. **Comandos separados.** `A B C D` disparan el pulso largo; `1 2 3 4` siguen
   siendo toques. En mayúsculas a propósito: **una errata no puede arrancar un
   viaje**.
2. **El pulso largo se acota a 2800 ms**, por debajo de los 3.0 s que graban un
   preset.
3. **El freno es cerrar el mismo canal.** No se depende de tener otro cableado.
4. ⚠️ **Solo con supervisión mientras no existan los límites por software.**

### El riesgo que esto añade, dicho claro

**Un cuelgue del ESP32 durante el viaje deja el escritorio moviéndose sin nada
que lo pare.** Antes eso era imposible por diseño; ahora es posible. El watchdog
de [ADR-024](DECISIONS.md) **no ayuda**: abre el canal, y abrir no frena.

Lo que queda como red: el **tope físico** en cada extremo, y el **mando**, que
sigue conectado y para con cualquier botón. **Esto es una mitigación, no una
garantía.**

**Lo que haría falta para quitar la supervisión:** límites por software con la
altura leída del bus, y una parada automática al detectar un movimiento no
pedido —el display parpadea al arrancar continuo, y eso ya está medido y sirve de
señal ([HARDWARE.md](HARDWARE.md))—.

---

## ADR-029 — El sistema no asocia M1 y M2 a ninguna altura: son botones opacos

**Fecha:** 2026-08-22
**Estado:** Aceptado

### Contexto

El 2026-08-22 se verificó qué memoria va con qué altura: **M1 → canal 3 → 80 cm**
y **M2 → canal 4 → 117 cm**. La tentación inmediata es meter esos números en la
integración, para que Home Assistant pueda decir "ir a M1 (80 cm)".

Contra eso hay dos incógnitas que **no se pueden cerrar por medición barata**:

1. **El botón de reset**, que no está cableado pero que una persona puede pulsar,
   ejecuta una recalibración. **Si recalibra el cero, las alturas de M1 y M2
   dejarían de significar lo que significaban.** Supuesto, no verificado.
2. **Cualquiera puede regrabar una memoria desde el mando** manteniendo el botón
   3 segundos. El sistema no tiene forma de impedirlo ni de enterarse salvo por
   el resultado.

En los dos casos, un sistema que tenga escrito "M1 = 80 cm" **estaría afirmando
algo falso sin saberlo**.

### Decisión

**Para el sistema, M1 y M2 son botones opacos.** Se exponen como "recuperar la
memoria 1" y "recuperar la memoria 2", **sin altura asociada**.

**Corolario:** los presets por software —alturas con nombre— son un mecanismo
**completamente independiente**. Se alcanzan con subir/bajar y freno por altura
leída del bus, ya demostrado el 2026-08-22 yendo a 95 cm, que no es ninguna
memoria del mando. **No usan M1 ni M2 y no pueden sobrescribirlas**: el pulso
largo está acotado a 2800 ms, por debajo de los 3.0 s que graban un preset
([ADR-028](DECISIONS.md)).

### Qué sí puede hacer el sistema

**Observar y publicar, que es distinto de configurar.** Tras recuperar una
memoria, el ESP32 ve en el bus a qué altura acabó el escritorio y puede
publicarlo como observación fechada: *"M1 acabó en 80 cm, hace dos días"*.

Eso es información honesta —dice lo que se vio y cuándo— y **degrada bien**: si
la memoria cambia, el siguiente uso actualiza la observación en vez de
contradecir una configuración.

### Qué se pierde

**Home Assistant no podrá decidir a priori si M1 es la altura que quiere.** Para
ir a una altura concreta se usa un preset por software, que es determinista.

Es poco a cambio de eliminar una clase entera de fallo silencioso: **el sistema
creyendo que sabe algo del hardware que el hardware ya no cumple.**

### De dónde salió

Lo propuso quien tiene el escritorio, al preguntar qué pasaría con el reset. El
razonamiento —"para el sistema será M1 y M2 y ya"— **es exactamente el
desacoplamiento correcto**, y se adopta tal cual.

---

## ADR-030 — El transporte a Home Assistant es MQTT, no ESPHome

**Fecha:** 2026-08-22
**Estado:** Aceptado

### Contexto

La fase 4 necesita llevar la altura y los controles a Home Assistant. Los dos
caminos razonables eran **ESPHome** —con el sniffer como componente externo— y
**firmware propio publicando por MQTT**.

**Lo que se encontró al inspeccionar la instalación real** (por SSH, 2026-08-22):

| | Estado |
|---|---|
| Home Assistant | 2026.5.2, **en Docker, red `host`** |
| Add-ons | **Imposibles**: es HA Container, sin Supervisor |
| ESPHome | Integración **ya funcionando**, con un M5Stack Atom Echo |
| InfluxDB | Integrado, apuntando a `pironman5` |
| MQTT | **No existía**: ni broker, ni integración, ni contenedor |

Con eso a la vista, la primera lectura fue que ESPHome ganaba: la integración ya
estaba, no hace falta broker y no añade servicios a Ultron.

### El argumento que decide, y que se pasó por alto al principio

**ESPHome llama al `loop()` de cada componente y espera que devuelva enseguida.
El sniffer hace lo contrario:**

- **Hasta 250 ms** esperando a que el bus baje, antes de cada ráfaga
  (`captureBurst`)
- **2800 ms enteros** durante un pulso largo, capturando dentro del bucle
  ([ADR-028](DECISIONS.md))

Un componente que bloquea 2.8 s **dispara el watchdog de ESPHome y le tira la
API**. Meterlo ahí obliga a **reescribir el sniffer como máquina de estados no
bloqueante** — precisamente el código con timing crítico que el 2026-08-21 costó
una tarde entera de depuración, y cuyo comportamiento está medido y verificado
tal como está.

### Decisión

**MQTT, con un broker Mosquitto en Ultron.** El firmware actual se conserva y
solo se le añade WiFi y publicación.

**Lo que cuesta**, y es todo:

- Un contenedor `eclipse-mosquitto:2` en Ultron, aislado, con su volumen propio,
  usuario y contraseña. Levantado y verificado el 2026-08-22.
- La integración MQTT de HA, que viene incluida.
- El *discovery* lo emite el propio firmware.

**Lo que no cambia:** InfluxDB recoge de HA, no de la fuente, así que las
estadísticas de largo plazo funcionan igual.

### Qué se pierde

**La integración nativa de ESPHome y su OTA.** El OTA se recupera con
`ArduinoOTA`. El descubrimiento automático lo da MQTT Discovery.

**Y se añade una pieza a Ultron** —el broker— donde ya corrían cuatro servicios.
Es el precio de no tocar el código que funciona.

### Nota de método

**Esta decisión se recomendó dos veces al revés antes de fijarla**: primero MQTT
por defecto, luego ESPHome al ver la instalación, y de vuelta a MQTT al pesar el
bloqueo. **La razón del vaivén fue recomendar antes de haber mirado**, y quedó
registrada aquí porque el ADR sin el vaivén parecería una decisión más limpia de
lo que fue.

---

## ADR-031 — El "no hay daño" del ADR-019 era incompleto: faltaba el latch-up

**Fecha:** 2026-08-23
**Estado:** Aceptado
**Corrige a:** [ADR-019](DECISIONS.md), que no se toca

### Contexto

El ADR-019 describe qué pasa con **el ESP32 sin alimentar y el bus encendido**:
el diodo de protección del GPIO engancha el nodo del divisor y arrastra el bus
hasta ~2.85 V, y **el mando falla**. Concluía:

> **No hay daño** —la corriente son ~210 µA, limitados por los 9.1 kΩ— pero el
> síntoma aparecería justo después de soldar.

La regla de conexión que salió de ahí es correcta y sigue vigente: **USB primero,
hilos del bus después; al desmontar, al revés.**

### Qué faltaba

**El razonamiento valoraba la magnitud de la corriente, no su naturaleza.** 210 µA
es poca corriente para calentar nada, y esa fue toda la evaluación.

Pero **inyectar corriente por un pin es el disparador clásico del latch-up**: una
estructura parásita en el silicio que se activa, conduce a masa y **solo se apaga
quitando la alimentación por completo**. No depende de disipar potencia; depende
de superar una corriente de disparo en la unión. **Un ADR que concluye "no hay
daño" sin haber considerado ese mecanismo no puede sostener esa conclusión.**

### Lo observado el 2026-08-23

El mando presenta un cuadro compatible con latch-up recurrente: revive solo tras
cortes de corriente largos, muere a la primera pulsación, y deja **CLK clavado en
0.7–0.9 V** mientras DIO sigue a 4.0 V. Con CLK sujeto abajo la caja de control
no puede generar reloj, y el bus queda mudo — cero flancos.

**No está demostrado que la causa fuera el escenario del ADR-019.** La condición
se dio varias veces el 2026-08-22 al cerrar la tapa, pero también hubo
soldaduras, una pulsación del botón de reset y horas de accionamiento. **La causa
queda como no determinada**, y esto se registra por lo que enseña, no como
veredicto.

### Decisión

1. **La frase "no hay daño" del ADR-019 queda retirada.** Lo correcto es: *"no
   hay daño térmico; el riesgo de latch-up no se evaluó"*.
2. **La regla de conexión sube de categoría.** Deja de ser una precaución contra
   un síntoma confuso y pasa a ser **protección del hardware**: nunca dejar el
   ESP32 sin alimentar con los hilos del bus conectados y el escritorio
   encendido. **Ni un momento.**
3. **Para el montaje futuro, evaluar aislar la sonda** — un buffer alimentado
   independientemente, o resistencias de valor mucho más alto — de modo que el
   estado "ESP32 muerto, bus vivo" no pueda inyectar nada.
4. ⚠️ **Al retomar con un mando nuevo, esto se resuelve ANTES de conectar la
   sonda.** Repetir el montaje tal cual es arriesgar el repuesto.

### Nota de método

**El fallo no fue no saber qué es el latch-up: fue dar por cerrada una
conclusión de seguridad tras evaluar una sola vía de daño.** "210 µA es poco"
respondía a la pregunta térmica y se tomó como respuesta a todas.

---

## ADR-032 — El ESP32 puede ocupar el sitio del mando: la premisa del ADR-011 ya no se cumple

**Fecha:** 2026-08-23
**Estado:** Aceptado
**Reabre:** [ADR-011](DECISIONS.md), que no se toca

### Contexto

El [ADR-011](DECISIONS.md) descartó que el ESP32 simulara pulsaciones en el bus,
y el razonamiento era correcto:

> El bus es open-drain: los participantes solo pueden tirar hacia abajo. Forzar
> un bit a 1 no es difícil, es eléctricamente imposible **sin pelearse con el
> chip**.

**Esa premisa era el AiP650 vivo y conectado.** Desde el 2026-08-23 el chip del
mando está dañado ([bitácora](BITACORA.md): latch-up recurrente) y el mando está
fuera del cable. **No hay con quién pelearse.**

Y hay otra frase del ADR-011 que también deja de aplicar: *"el chip es
irreemplazable y el mando entero con él"*. El chip es un `AiP650E` / `TM1650`,
que cuesta menos de un dólar; lo difícil de conseguir es el **mando**, no el
chip.

### La evidencia nueva

**El pull-up del bus está en la caja de control**, medido el 2026-08-23 con el
mando desconectado: **21 kΩ en CLK y 22 kΩ en DIO** contra los 5 V.

Eso es lo que hacía falta: un sustituto **solo necesita tirar hacia abajo**. El
nivel alto lo pone la caja sola. Es exactamente lo que un GPIO sabe hacer.

### Decisión

**Se admite que el ESP32 escriba en el bus, sustituyendo al mando**, con estas
condiciones:

1. ⚠️ **Solo con el mando FUERA del cable.** Con un mando funcionando conectado,
   el ADR-011 sigue vigente en su totalidad: **no se escribe**.
2. **Nunca con salida push-pull.** El GPIO tira abajo o queda en alta
   impedancia, jamás fuerza un alto. Esa prohibición del ADR-011 se mantiene.
3. **Hace falta adaptación de nivel.** El bus va a 5 V y los GPIO aguantan
   3.6 V. Leer ya está resuelto con el divisor; **escribir necesita un MOSFET o
   transistor por línea** — `2N7000`, `BS170`, `BSS138`, `2N3904`, `BC547`. No
   hay ninguno en el inventario.
4. **Los PC817 no sirven aquí**: conmutan en el orden de 10 µs y cada bit del
   bus dura ~5 µs. Demasiado lentos.

### Lo que no está demostrado

**Que el ESP32 llegue a tiempo.** Hacer de esclavo a 202 kHz es más exigente que
escuchar. A 240 MHz hay ~1200 ciclos por bit, que parece margen suficiente, pero
**no se ha probado**.

**Probarlo no puede dañar nada más**: si responde tarde, la caja no lee tecla.
No hay conflicto eléctrico posible, que era justo el peligro del ADR-011.

### Qué se gana

**Una salida que no depende de conseguir un mando.** Ni `JK-CH506` ni
`FELV3-F4.0` aparecen en catálogos públicos: son códigos de fabricación, y
buscar repuesto por referencia no lleva a ninguna parte.

Todo lo hecho hasta ahora sirve tal cual: el protocolo descifrado, los cuatro
códigos de tecla verificados, los tiempos medidos y la integración con Home
Assistant.

### Alternativa que sigue viva

**Sustituir el chip del mando.** Un `TM1650` cuesta menos de un dólar y devuelve
el mando original a la vida sin firmware nuevo. Requiere soldadura SMD. **Las
dos rutas son baratas y conseguibles localmente**, y no se excluyen.

---

## ADR-033 — El ADR-032 queda en suspenso: el mando no estaba dañado

**Fecha:** 2026-08-23
**Estado:** Aceptado
**Suspende:** [ADR-032](DECISIONS.md), que no se toca

### Contexto

El [ADR-032](DECISIONS.md) admitía que el ESP32 escribiera en el bus para
sustituir al mando. Su condición era explícita: **solo con el mando fuera del
cable**, porque se le creía dañado sin remedio.

**El mando no estaba dañado.** Era un cortocircuito entre el hilo verde (DIO) y
el amarillo (5 V), un puente de estaño de las soldaduras hechas al cerrar la
tapa. Deshecho, el mando funciona: se grabaron y recuperaron dos memorias, que
exige mantener pulsado 3 s con lectura sostenida del teclado.

### Decisión

**El ADR-032 queda en suspenso.** Su condición de activación —mando muerto y
fuera del cable— no se cumple.

**El [ADR-011](DECISIONS.md) vuelve a aplicar entero: el bus es exclusivamente
de lectura.** Con un mando vivo conectado, escribir es pelearse con su chip, y
sigue prohibido.

El ADR-032 **no se retira**: su razonamiento y la medición del pull-up siguen
siendo válidos, y volvería a aplicar si algún día el mando muere de verdad.

### Lo que se conserva

**El pull-up de la caja: 21 kΩ en CLK, 22 kΩ en DIO**, medido con el mando
desconectado. Es el único hallazgo útil del episodio y explica los 2.4 kΩ del
2026-08-06, que eran estos en paralelo con los del mando.

**No hace falta comprar nada.** Los transistores del ADR-032 quedan sin objeto.

### La lección, que es de método

Se construyó un diagnóstico elaborado —latch-up, estructuras parásitas, daño
permanente, mando irrecuperable— **sobre dos mediciones de tensión**, en un
montaje **que se acababa de manipular a mano**. La explicación simple —un puente
de estaño entre dos hilos contiguos— no se buscó primero.

**Regla:** cuando algo falla justo después de tocar el hardware, **el primer
sospechoso es lo que se tocó**, y se descarta mirando y midiendo antes de
razonar sobre el silicio.

Es la segunda vez en la misma sesión: el 2026-08-21 un canal "muerto" resultó
ser un fallo del propio sniffer.

---

## ADR-034 — Actualización de firmware por red (OTA), rechazada durante el movimiento

**Fecha:** 2026-08-23
**Estado:** Aceptado

### Contexto

Desde que el ESP32 pasó a un cargador de pared, cada corrección exigía
desenchufar el escritorio, llevar el USB al Mac y devolverlo. **Ese baile es en
sí mismo el peligro del [ADR-019](DECISIONS.md)**: cada tránsito deja momentos
con el ESP32 sin alimentar y la sonda sobre un bus vivo, la condición que ya
causó dos días de diagnósticos falsos.

O sea: el procedimiento para arreglar el firmware era más arriesgado que casi
cualquier fallo del firmware.

### Decisión

**Se implementa OTA con `ArduinoOTA`**, protegido por contraseña
(`OTA_PASSWORD` en `secrets.h`, no versionada), y **se publica la IP del
dispositivo** como entidad de diagnóstico — sin ella no se sabe a dónde
actualizar.

⚠️ **Y se RECHAZA la actualización si el escritorio está en movimiento.**

Esa guarda no es celo: aplicar una actualización **reinicia el chip**, el
reinicio deja los GPIO como entradas y **abre los canales** — y abrir un
contacto **no detiene el movimiento continuo** (medido: 6 cm en 6.6 s tras
soltar). Una actualización a mitad de viaje dejaría el escritorio corriendo
hasta su tope sin nadie supervisando. Si llega una petición con `g_motion`
distinto de `MOTION_IDLE`, se aborta antes de escribir un solo byte.

### Por qué es seguro contra interrupciones

El ESP32 escribe la imagen en la **partición inactiva** y solo cambia el destino
de arranque cuando la imagen está completa y verificada. **Una transferencia
cortada deja intacto el firmware que corre.** No hay estado intermedio en el que
el aparato quede inservible.

### Qué se gana y qué se paga

**Se gana** eliminar la manipulación física recurrente — la fuente de riesgo
número uno de este proyecto, con dos incidentes documentados.

**Se paga:** el programa sube del 69% al 74% de la flash, y **se abre un puerto
en la red local**. Mitigado con contraseña; la red es doméstica y el peor caso
de un atacante con acceso a ella y a la contraseña es mover un escritorio.

**Verificado el 2026-08-23**: actualización completa por red a `192.168.1.23`,
autenticada, con el ESP32 reconectando solo al broker tras reiniciar.

**Corrección del mismo día (revisión adversarial de la implementación):** el
mecanismo de rechazo original era `ESP.restart()` al detectar movimiento — es
decir, **el rechazo ejecutaba exactamente la fuga que decía evitar**: reiniciar
abre los canales y abrir no frena. Desde la corrección, una OTA durante un viaje
**frena primero** (un toque real, el mismo del `parar`) y deja pasar la
actualización sobre un escritorio que ya se está deteniendo.
