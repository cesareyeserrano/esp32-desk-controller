# Hardware

> Hechos físicos verificados. Cada afirmación indica si está **verificada**
> (medida u observada) o es un **supuesto**.

---

## Inventario

- **ESP32 DevKit 38 pines** sobre placa de expansión con borneras de tornillo.
  Serigrafía: `HW-395 V0.0.3` / `DT-Y6593`.
- **Módulo de 4 relés** Songle SRD-05VDC-SL-C, con optoacopladores y jumper
  JD-VCC.
- Resistencias surtidas de 1/4 W, ver tabla abajo.
- Multímetro. Cautín y estaño.
- **No hay** osciloscopio ni analizador lógico. Esto condiciona todo: el propio
  ESP32 tiene que hacer de instrumento de medida.
- MacBook Air M4 para programar por USB.

---

## El escritorio y su mando

Escritorio elevable de columna motorizada, marca comercial **Cougar**. Cougar es
marca de periféricos, no fabricante de actuadores: el mecanismo es maquilado, y
la serigrafía `JK-CH506` del mando apunta a **Jiecang**. Buscar documentación
por "Cougar" no lleva a ninguna parte; por Jiecang o por TM1650, sí.

**Dos columnas motorizadas**, unidas entre sí por un cable de **6 hilos**. El
cerebro está en la caja de control. **No se ha abierto y no hace falta
abrirla.**

Alimentación: adaptador externo de **29 V** *(verificado, leído en la etiqueta
del adaptador)*. La caja de control baja esos 29 V a los 5 V que alimentan el
mando.

Ese cable de 6 hilos entre motores lleva con toda probabilidad **los 29 V** y
los sensores Hall. **No se toca**: los Hall darían posición contando pulsos, que
es justo lo que descartó [ADR-001](DECISIONS.md), y hacerlo obligaría a pinchar
cableado con corriente de motor a 29 V. El display ya da altura absoluta y en
centímetros.

### Conectores externos de la caja de control — VERIFICADO

*Comprobado por inspección visual el 2026-08-03, sin desmontar nada.*

| Conector | Qué es |
|---|---|
| Entrada del adaptador | 29 V |
| Conector del mando | 4 hilos |
| Cable de motores | 6 hilos |

### El pull-up del bus vive en la caja de control — VERIFICADO 2026-08-23

Medido **con el mando desconectado**, sobre el cable que sale de la caja:

| Entre | Resistencia |
|---|---|
| Rojo (CLK) ↔ amarillo (5 V) | **21 kΩ** |
| Verde (DIO) ↔ amarillo (5 V) | **22 kΩ** |

**Las dos líneas tienen pull-up en la caja.** Encaja con los 2.4 kΩ medidos el
2026-08-06 *con el mando conectado*: eran estos ~21 kΩ en paralelo con otro más
fuerte dentro del mando.

**Consecuencia:** un sustituto del mando **solo necesita tirar de las líneas hacia
abajo**; el nivel alto lo pone la caja. Es la condición que hacía falta para que
el ESP32 pueda ocupar el sitio del mando. Ver [ADR-032](DECISIONS.md).

**Referencia leída en el motor el 2026-08-23: `FELV3-F4.0`.** Buscada sin
resultado: no aparece en catálogos públicos ni en distribuidores. **Es código de
fabricación, como `JK-CH506`** — el mismo patrón. No sirve para localizar
repuestos por referencia.

**No hay ningún otro.** En concreto, **no existe puerto de accesorios**
RJ11/RJ12/RJ45. Eso descarta el atajo del paso C de [PLAN.md](PLAN.md) —esas
cajas Jiecang con puerto serie 9600 8N1 y componentes ESPHome ya hechos— y
confirma que **el bus del mando es el único camino**. Era lo esperable: un mando
con AiP650E indica una caja de gama sencilla.

Que sean dos motores tiene otra consecuencia:
- Las dos columnas **pueden desincronizarse** tras un corte de luz o una
  sobrecarga, y el escritorio queda torcido. Recalibrar es para lo que sirve el
  botón de reset. Refuerza [ADR-008](DECISIONS.md): ese botón se queda fuera del
  circuito y la recalibración es siempre manual, mirando debajo del escritorio.

### Mando / handset

- PCB marcada `JK-CH506 Rev1.2` y `G0088-30-4137-202518` (segunda lectura sobre
  fotografía; el handover original transcribió `G9008B-3E-4137-202518`). El
  sufijo `202518` encaja con un código de fecha: **semana 18 de 2025**.
  "JK" apunta a **Jiecang**, fabricante chino de actuadores para escritorios.
- `JK-CH506` **no aparece en el catálogo público de Jiecang**, cuyos mandos se
  llaman `JCHT35Kxx`. Es referencia de fabricación o de cliente, no de catálogo:
  no hay documentación oficial que buscar. La documentación útil es la del chip.
- 5 pulsadores táctiles y un display de 7 segmentos con matriz de LEDs que
  muestra la altura actual del escritorio.
- Chip único: **AiP650EO**, marcado `19BT450`, SOP-16. `19BT450` es código de
  lote, no variante. Clon funcional del **TM1650** de Titan Micro; el fabricante
  del clon es I-CORE (Wuxi), que es el logotipo de barras verticales del
  encapsulado. Driver de display de **8 segmentos × 4 dígitos, cátodo común**,
  con escáner de teclado 7×4 integrado y soporte de algunas combinaciones.
  Interfaz de dos hilos tipo I2C. Alimentación 3–5.5 V. Pinout y protocolo
  completos en [PROTOCOLO.md](PROTOCOLO.md).
- **No hay microcontrolador en el mando.** *Verificado por inspección: el
  AiP650EO es el único chip de la placa.* Esto es lo que hace viable todo el
  proyecto — significa que la caja de control envía por el cable los dígitos
  literales que se ven en pantalla.
- Se conecta a la caja de control con un cable de **4 hilos**, conector JST
  blanco de 4 pines, paso pequeño (tipo PH o ZH).

---

## Pinout del cable — VERIFICADO

*Verificado el 2026-08-02 por continuidad, con el conector desenchufado de la
caja de control y el multímetro en ohmios. Las cuatro lecturas dieron 0.2 Ω, que
es resistencia de pista: son conexiones directas, no caminos indirectos.*

| Color | Pata del AiP650 | Función | Estado |
|---|---|---|---|
| **Rojo** | 2 | **CLK** (SCL) — reloj del bus | **Verificado** |
| **Verde** | 3 | **DIO** (SDA) — datos del bus | **Verificado** |
| **Azul** | 4 | **GND** — tierra | **Verificado** |
| **Amarillo** | 10 | **VDD** — 5 V | **Verificado** |

Los cuatro hilos caen exactamente sobre las cuatro funciones que un cable de
mando tiene que llevar, y coinciden con el pinout del datasheet de I-CORE. El
fabricante llama a las líneas **CLK** y **DIO**, no SCL/SDA.

### Corrección: el handover se equivocaba con el rojo y el amarillo

El [handover original](historia/HANDOVER-2026-07-27.md) daba el **rojo** como
"VCC (5 V) probable" y el amarillo y el verde como las líneas de datos. Es al
revés en dos de los cuatro:

| Hilo | Handover decía | Es en realidad |
|---|---|---|
| Rojo | VCC (supuesto) | **SCL** |
| Amarillo | Datos | **VDD, 5 V** |
| Verde | Datos | SDA — acertado |
| Azul | GND | GND — acertado |

Consecuencia si se hubiera montado el plan viejo: el **reloj habría quedado sin
conectar** —el sniffer no habría capturado nada— y la **línea de 5 V habría ido
a un GPIO** por un divisor. Dos fallos a la vez, y el síntoma (no se ve tráfico)
no apuntaba a ninguno de los dos.

El error venía de que un multímetro no distingue una línea de datos en reposo
alto de una línea de alimentación: las tres no-azul miden ~4.x V igual. Por eso
el handover lo marcaba como *supuesto*, y por eso no se montó.

**Por tanto:** los divisores van en **rojo** y **verde**. El hilo que **no se
conecta** es el **amarillo**. El azul sigue yendo a GND.

Nota: continuidad al chasis metálico del motor, ninguno de los 4 hilos da
continuidad. Normal — fuente flotante o chasis pintado.

---

## Los pulsadores

*Función verificada por uso del mando.*

| Botón | Función | ¿Automatizable? |
|---|---|---|
| Subir | Sube mientras se mantiene pulsado | Sí |
| Bajar | Baja mientras se mantiene pulsado | Sí |
| M1 | Va a la altura guardada 1 | Sí — es la vía preferente |
| M2 | Va a la altura guardada 2 | Sí — es la vía preferente |
| Reset | Recalibración | **No. Nunca.** Ver [ADR-008](DECISIONS.md) |

### Comportamiento de los botones

*Verificado por uso del mando. **Los tiempos ya están medidos** — ver
[Tiempos de accionamiento por relé](#tiempos-de-accionamiento-por-relé) más
abajo: recuperar un preset dura < 0.2 s y grabarlo exige 3.0 s.*

| Acción | Resultado |
|---|---|
| Pulso corto en subir/bajar | Mueve ≈ 1 cm y para |
| Mantener subir/bajar | Mueve hasta el límite físico |
| Cualquier botón durante el movimiento | **Detiene el movimiento** |
| Pulso corto en M1/M2 | Va a la altura guardada |
| **Mantener M1/M2** | **Graba la altura actual en ese preset** |

Dos consecuencias que gobiernan el diseño del accionamiento:

**El mismo botón que recupera un preset lo sobrescribe si se mantiene.** Un relé
que cierre de más no va a la posición guardada: la destruye. El umbral de tiempo
que separa "ir a" de "grabar" **hay que medirlo** y los pulsos deben quedar muy
por debajo. Ver [ADR-010](DECISIONS.md).

**"Cualquier botón detiene" es un freno.** Cualquier relé disponible sirve para
abortar un movimiento en curso.

Detalle físico:

- Pulsadores táctiles de 4 patas. Las patas del mismo lado están unidas
  internamente; las **diagonales** son el par que el botón cierra al presionar.
- **Verificado: entre las patas de un pulsador hay 5 V.**
- Consecuencia: no pueden conectarse a un GPIO del ESP32, que tolera 3.6 V.
  Ver [ADR-004](DECISIONS.md).

---

## Punto de derivación

En la placa del mando, los 4 pines del conector JST están soldados y accesibles
por el reverso. Se sueldan cablecitos finos (AWG 28–30) **dejando el conector
original puesto**, para que el mando siga funcionando durante todo el proceso.

**Solo tres hilos. El amarillo no se suelda.**

| Pin del conector | Función | A dónde va |
|---|---|---|
| Donde entra el **rojo** | CLK | Divisor → P18 |
| Donde entra el **verde** | DIO | Divisor → P4 |
| Donde entra el **azul** | GND | GND del ESP32 |
| Donde entra el **amarillo** | VDD 5 V | **Nada. No se suelda.** |

Dejar el amarillo sin soldar no es solo ahorrarse una junta: evita tener un hilo
suelto con 5 V cerca de una protoboard llena de GPIO que toleran 3.6 V.

### Procedimiento

**Preparación.** Escritorio desenchufado de la corriente. Placa fuera de la
carcasa y sujeta a la mesa. Tres hilos de 20–30 cm, de colores distintos,
**anotando qué color va a qué pin** — una vez en la protoboard ya no se
distinguen.

Las juntas del conector ya tienen estaño, así que basta refundirlas y apoyar el
hilo estañado. Poco tiempo de cautín por junta: insistir con la punta apoyada es
como se levanta una pista.

### Alivio de tracción — no es opcional

El fallo que arruina el mando no es la soldadura, es el tirón: un hilo fino
soldado a una pista, si alguien lo estira, **arranca la pista**. Nada de lo que
viene después sirve si eso pasa.

En cuanto estén los tres hilos, **fijarlos a la placa con cinta a un par de
centímetros de las soldaduras**, de modo que cualquier tirón lo aguante la cinta
y no el cobre.

### Verificación antes de cerrar

Se aprovecha que el mapa del chip ya está verificado:

| Hilo nuevo | Debe dar continuidad con la pata |
|---|---|
| El del pin rojo | 2 (CLK) |
| El del pin verde | 3 (DIO) |
| El del pin azul | 4 (GND) |

Sin continuidad → soldadura fría. Y comprobar además que **no** hay continuidad
entre hilos vecinos: un puente de estaño entre dos pines del conector es fácil
de hacer y no se ve a simple vista.

**Después, montar la carcasa y usar el mando normalmente.** Subir, bajar, mirar
el display. Si funciona igual que antes, las derivaciones están bien. Si algo
cambió, se resuelve **antes** de conectar el ESP32 — después ya no se sabría a
quién culpar.

### Si se levanta una pista

No es el fin. La pista del conector va a la pata del chip, así que se puede
soldar el hilo directamente a la pata correspondiente del AiP650E. Es más
difícil —paso de 1.27 mm— pero es la misma señal. Antes de intentarlo, verificar
con el multímetro adónde llega lo que quede de pista.

---

## Resistencias disponibles

**Todas medidas con multímetro el 2026-08-02.** Sustituye por completo al
inventario anterior, que estaba leído del código de colores y tenía al menos
tres valores equivocados. 30 piezas.

| Valor medido | Cantidad |
|---|---|
| 2 Ω | 2 |
| 4.5 Ω | 3 |
| 5.4 Ω | 2 |
| 6.7 Ω | 2 |
| 36 Ω | 2 |
| 54 Ω | 2 |
| 75 Ω | 2 |
| 81 Ω | 2 |
| 555 Ω | 1 |
| 800 Ω | 1 |
| **1.8 kΩ** | 3 |
| **3.3 kΩ** | 1 |
| **7.4 kΩ** | 2 |
| **9.1 kΩ** | 2 |
| 74 kΩ | 1 |
| 240 kΩ | 1 |
| 2.2 MΩ | 1 |

**Compradas y recibidas el 2026-08-03**, medidas con multímetro antes de usarse
según exige [ADR-015](DECISIONS.md):

| Valor medido | Para qué |
|---|---|
| **26.79 – 27.01 kΩ** (lote) | Abajo del divisor de la sonda ([ADR-016](DECISIONS.md)) — **desbloquea la fase 2** |
| 10 kΩ | Pull-downs de los GPIO de control, fase 3 |
| 330 Ω | LED de entrada de los optoacopladores, fase 3 |

**Las de 27 kΩ, medidas pieza a pieza, caen entre 26.79 y 27.01 kΩ.** Con las
9.1 kΩ del cajón arriba, el divisor queda:

| Con R abajo de | Nivel alto del bus | Tensión en el GPIO |
|---|---|---|
| 26.79 kΩ | 3.99 V | 2.98 V |
| 27.01 kΩ | 3.99 V | 2.99 V |

Clavado en el diseño de ADR-016. Los extremos del lote se separan 10 mV en el
GPIO, así que **da igual qué dos piezas se usen** — y como difieren un 0.8% entre
sí, los dos canales se comportan igual. Impedancia en flanco de subida: 10.8 kΩ.

Confirma además el cociente esperado en la comprobación de
[ADR-018](DECISIONS.md): 3.99 / 5 = **0.80**.

### De dónde venía la confusión

Las dos piezas que se leyeron como marrón-verde-naranja (15 kΩ) y luego como
violeta-verde-naranja (75 kΩ) son en realidad **violeta-verde-rojo, 7.4 kΩ**.
De 74 kΩ solo hay una, la que ya estaba contada.

Tres lecturas de color resultaron equivocadas antes de medir, y dos de ellas
cambiaron el diseño de la sonda — una lo desbloqueó y otra lo anuló. La causa no
es descuido: el código de colores **no tiene redundancia**, y un color mal leído
mueve el valor un factor de mil. De ahí la regla de [ADR-015](DECISIONS.md): los
valores se miden, no se leen.

### Por qué no alcanza, y por cuánto

Cada divisor necesita sumar unos **26 kΩ** para que el nivel alto del bus quede
por encima de 3.7 V con el pull-up interno de 9.1 kΩ. Dos divisores: 52 kΩ.

Todo el material entre 1 kΩ y 10 kΩ suma **41.7 kΩ** — 3 × 1.8 k, 1 × 3.3 k,
2 × 7.4 k y 2 × 9.1 k. Faltan unos diez.

*Precisión registrada: antes aquí ponía 43 kΩ. Esa cifra incluía las de 800 Ω y
555 Ω, que están por debajo de 1 kΩ y no sirven para esto. La conclusión —no
alcanza— no cambia.*

La de 74 kΩ no rescata la situación: permite armar una línea correcta —74 kΩ
abajo con 36 kΩ arriba hechos de 9.1 + 9.1 + 7.4 + 7.4 + 3.3— pero consume todo
el material medio, y la segunda línea se queda con 7 kΩ, que dejan el bus en
2.2 V. Inservible.

### Qué comprar

⚠️ **Corregido el 2026-08-22: el diagrama de abajo decía 9.1 kΩ arriba, y lo
montado son 16.3 kΩ** ([ADR-022](DECISIONS.md), que reemplazó al ADR-016 el
2026-08-06). El texto no se actualizó entonces, y el error se propagó a las
cabeceras de las capturas escritas ese día. **Medido sobre el montaje real: 16.5
kΩ entre el hilo del bus y el GPIO, y 43.5 kΩ del bus a masa** — que es
16.3 + 27, las dos en serie. El valor bueno es el del ADR-022.

**Dos resistencias, y se monta.** Las medidas van arriba en ambas líneas;
solo faltan las de abajo.

| Compra | Nivel alto del bus | Tensión en el GPIO |
|---|---|---|
| **2 × 27 kΩ** ← preferida | 3.99 V | 2.99 V |
| 2 × 22 kΩ | 3.87 V | 2.74 V |
| 2 × 33 kΩ | 4.12 V | 3.23 V |

Las tres cumplen (bus ≥ 3.5 V, GPIO entre 2.5 y 3.6 V). La de 27 kΩ deja ambas
tensiones más centradas.

Sigue mereciendo la pena un **surtido etiquetado** (600–1000 piezas, 5–8 €): las
fases 3 y 4 pedirán más resistencias.

---

## Circuito de sonda

### Diseño válido — [ADR-016](DECISIONS.md)

Pendiente de dos resistencias de 27 kΩ. El resto ya está.

**Plano: [hardware/plano_sonda_v2.svg](hardware/plano_sonda_v2.svg)** — es el
único que se monta.

```
ROJO  (CLK) ──[16.3k]─┬──> P18 del ESP32
                      └──[27k]──> GND

VERDE (DIO) ──[16.3k]─┬──> P4 del ESP32
                      └──[27k]──> GND

AZUL  (GND) ──────────────> GND del ESP32

AMARILLO (5 V) ───────────  NO SE CONECTA
```

| | Valor | Exigido |
|---|---|---|
| Carga sobre el bus | 36.1 kΩ | — |
| Nivel alto del bus | 3.99 V | ≥ 3.5 V |
| Tensión en el GPIO | 2.99 V | ≥ 2.5 V y ≤ 3.6 V |
| Impedancia vista por el GPIO, flanco de **bajada** | 6.8 kΩ | — |
| Impedancia vista por el GPIO, flanco de **subida** | **10.9 kΩ** | — |

El GPIO va **al nodo entre las dos resistencias**, nunca a un extremo.

**Corrección registrada.** ADR-016 y este archivo daban 6.8 kΩ como impedancia,
sin más. Ese valor solo vale mientras el chip tira la línea abajo; en el flanco
de subida la fuente es el pull-up interno, que queda en serie con la resistencia
de arriba, y la impedancia real es (9.1 + 9.1) ∥ 27 = **10.9 kΩ**. El flanco
lento de un bus open-drain es siempre el de subida, así que ese es el número que
manda. No cambia el diseño ni el orden de preferencia entre las versiones de la
sonda. Detalle en [ADR-018](DECISIONS.md).

### Comprobación al conectarlo — por cociente, no por valor absoluto

**Un multímetro promedia, y el bus está conmutando.** Las dos lecturas van a
salir por debajo de lo calculado, en una cantidad que depende del ciclo de
trabajo del bus y que no se conoce hasta la primera captura. Por eso el criterio
no es un umbral absoluto sino la relación entre las dos medidas
([ADR-018](DECISIONS.md)):

1. **Sin la sonda conectada**, escritorio encendido y quieto, display mostrando
   un número estable: medir rojo↔azul y verde↔azul. Anotar.
2. **Con la sonda conectada**, sin tocar ningún botón y con el display mostrando
   el mismo número: medir los mismos dos puntos. Anotar.
3. Dividir la segunda lectura entre la primera, en cada línea.

| Cociente | Nivel alto real del bus | Qué hacer |
|---|---|---|
| **≥ 0.75** | ≥ 3.75 V | Correcto, seguir. Lo esperado es 0.80 |
| 0.70 – 0.75 | 3.50 – 3.75 V | Funciona, margen fino. Parar y pensar |
| **< 0.70** | **< 3.50 V** | **Desconectar.** Por debajo del VIH del chip |

Si entre las dos medidas el escritorio se movió o se pulsó algo, el ciclo de
trabajo cambió y el cociente no significa nada: repetir.

De propina, el cociente da el pull-up interno real, que hasta ahora solo se tenía
del datasheet y como valor típico: `Rpull-up = 36.1 kΩ · (1 - cociente) / cociente`.

**El ESP32 tiene que estar alimentado por USB antes de conectar los hilos al
divisor** ([ADR-019](DECISIONS.md)). Con el ESP32 sin corriente y la sonda
puesta, el bus se hunde hasta ~2.85 V y el mando falla — sin daño, pero con un
síntoma que apunta a las soldaduras.

Diseños anteriores, conservados como historia: [ADR-013](DECISIONS.md) (15 k /
33 k, válido pero sin componentes), [ADR-014](DECISIONS.md) (anulado, se apoyaba
en resistencias mal identificadas) y el original del handover de 800 Ω / 1.72 kΩ,
descartado por [ADR-005](DECISIONS.md).

⚠️ El plano de ese último diseño se renombró el 2026-08-03 a
[hardware/plano_divisores_v1_NO_MONTAR.svg](hardware/plano_divisores_v1_NO_MONTAR.svg),
porque convivía con el vigente bajo un nombre parecido y el riesgo de abrir el
equivocado era real justo antes de montar. **No se monta por dos motivos
independientes:** los valores están descartados por [ADR-005](DECISIONS.md), y
además manda el divisor al hilo **amarillo**, que resultó ser los 5 V, dejando el
reloj sin conectar.

En el nombre antiguo queda un archivo redirector, porque ADR-005 enlaza el plano
por nombre y los ADR no se editan.

**Solo hay un plano vigente:
[plano_sonda_v2.svg](hardware/plano_sonda_v2.svg).**

---

## Tiempos de accionamiento por relé

Del datasheet, y son los que acotan el pulso de un relé por los dos lados:

| Límite | Valor | De dónde sale |
|---|---|---|
| **Mínimo** para que la caja vea la pulsación | **~160 ms** | El chip exige que dure al menos dos periodos de escaneo, y el periodo llega a 80 ms [datasheet] |
| **Máximo** en M1/M2, antes de que grabe el preset | **3.0 s** | **Medido en el bus el 2026-08-06** |

Un pulso demasiado corto no hace nada y parece un fallo de cableado. Uno
demasiado largo en M1 o M2 sobrescribe el preset en silencio.

### El umbral de grabación, medido — VERIFICADO

*Captura [2026-08-06-umbral-grabar-memoria.log](capturas/2026-08-06-umbral-grabar-memoria.log).
Se midió manteniendo pulsada la memoria cuyo preset **ya valía la altura actual**,
para que grabar no destruyera nada.*

Contado sobre el bus, no con cronómetro: la caja lee el teclado cada ~200 ms, así
que esa es la resolución.

```
16.781 s   primera lectura con la tecla pulsada
   ...     15 lecturas seguidas, display fijo en '080'
19.778 s   el display se apaga por primera vez  <- HA GRABADO
20.378 s   parpadeo
20.977 s   parpadeo
21.576 s   parpadeo
22.175 s   parpadeo
```

**Del inicio de la pulsación al primer parpadeo: 2.997 s.** La confirmación de
grabado son **cinco parpadeos cada 0.6 s**, y siguen aunque se suelte el botón.

### Rango de alturas — VERIFICADO

*Captura [2026-08-06-topes-fisicos.log](capturas/2026-08-06-topes-fisicos.log),
recorriendo hasta los dos topes mecánicos.*

| | |
|---|---|
| Tope inferior | **73 cm** |
| Tope superior | **118 cm** |
| Recorrido | **45 cm** |

**Al topar no ocurre nada especial:** el display sigue mostrando un número, no
parpadea, no aparece ningún comando nuevo en el bus, y apenas frena — mantiene
~1.2 s por centímetro hasta el final. **Llegar al tope es indistinguible de estar
parado**, salvo porque la altura deja de cambiar.

Estos son los números para los límites por software de la fase 4
([SEGURIDAD.md](SEGURIDAD.md)).

### Subir y bajar: toque contra movimiento continuo — VERIFICADO

*Captura [2026-08-06-umbral-toque-vs-continuo.log](capturas/2026-08-06-umbral-toque-vs-continuo.log).*

Subir y bajar tienen **dos regímenes**, y solo uno es seguro:

| | Qué hace | ¿Se para solo? |
|---|---|---|
| **Toque corto** | Mueve ~1 cm | **Sí** |
| **Mantener y soltar** | Arranca movimiento continuo | **No.** Sigue hasta que se pulse cualquier botón |

**El umbral está entre 2.2 y 2.6 s**, acotado pulsando cada vez más largo:

| Duración | Resultado |
|---|---|
| 200 ms · 1.0 s · 1.6 s · 1.8 s | Toque |
| **2.2 s** | Toque — la más larga que se paró sola |
| **2.6 s** | **Continuo** — la más corta que se disparó |
| 2.8 s | Continuo |

⚠️ **Soltar no para el movimiento continuo.** Medido: siguió **5 cm en 5 s**
subiendo y **6 cm en 6.6 s** bajando después de soltar. Para detenerlo hay que
**cerrar** un contacto, no abrirlo.

**Corregido el 2026-08-21:** esta frase decía *"solo paró al pulsar **otra**
tecla"*, mientras la tabla de arriba decía *"cualquier botón"*. La imprecisa era
esta. **Medido bajando, con un solo canal cableado: un toque corto en el MISMO
botón para su propio movimiento continuo** — paró en 76 cm y siguió quieto 8 s.
Importa porque, si hiciera falta un botón distinto, un solo canal no podría
frenar nunca. Ver [ADR-028](DECISIONS.md).

**Los toques que "mueven ~1 cm" son de 1.0 s o más.** El más corto que se
cronometró el 2026-08-06 fue de 200 ms y **no se anotó cuánto movía**. Medido el
2026-08-21: **con 300 ms el escritorio no se mueve** —siete pulsos, cero
centímetros— aunque la caja sí registra la tecla. Con **800 ms** baja cerca de
1 cm cada tres pulsos. Ver [ADR-027](DECISIONS.md).

**El display parpadea cuando arranca movimiento continuo.** Observado a ojo y
confirmado en el bus: 0 parpadeos tras el toque de 2.2 s, 2 y 3 tras los
continuos. Sirve para que el firmware detecte un movimiento que no pidió.

### Ventana de trabajo para el accionamiento

| | Duración | Fuente |
|---|---|---|
| Mínimo para que el chip la vea | **160 ms** | Dos periodos de escaneo [datasheet] |
| **Ancho de pulso elegido** | **800 ms** | [ADR-027](DECISIONS.md), corrige los 300 ms de [ADR-023](DECISIONS.md) |
| Pulso largo, para arrancar continuo | **2800 ms** | [ADR-028](DECISIONS.md) |

### Qué sobrevive a un corte de corriente — VERIFICADO 2026-08-22

Se desenchufó el escritorio y se volvió a enchufar, con el ESP32 alimentado por
USB todo el rato.

| | Resultado |
|---|---|
| **La altura** | **Se conserva.** Estaba en 117 cm y siguió en 117 |
| **Las memorias del mando** | **Se conservan.** M1 llevó el escritorio a 80 cm, exacto |
| **El display** | Estaba dormido al volver a mirar |

### Corte de corriente EN PLENO MOVIMIENTO — VERIFICADO 2026-08-22

La prueba anterior se hizo con el escritorio quieto. Esta se hizo **cortando la
corriente de los dos —escritorio y ESP32— mientras el escritorio bajaba**, a los
95 cm. Bajando a propósito: si reanudara el movimiento, el tope de 73 cm lo
detiene.

| | Resultado |
|---|---|
| **¿Reanuda el movimiento al volver la luz?** | **NO.** Se queda quieto |
| **¿Conserva la altura?** | **Sí**, 95 cm — la posición no se pierde ni en marcha |
| **¿Revive el bus solo?** | **Sí**, 475 transacciones en 18 s sin intervención |
| **¿El display arranca encendido?** | **No.** Arranca **apagado** |

**Que no reanude el movimiento es el resultado importante.** Si lo hiciera, al
volver la luz el escritorio arrancaría solo con el ESP32 todavía arrancando y
**nadie capaz de frenarlo**. No ocurre.

**Anotado sin explicación:** tras un arranque en frío el byte de teclado en
reposo es `0x2E` (`KEY_NONE`) de forma constante, mientras que en caliente
alterna entre `0x07`, `0x17`, `0x27` y `0x2F`.

Captura:
[capturas/2026-08-22-corte-luz-en-movimiento.log](capturas/2026-08-22-corte-luz-en-movimiento.log).

### Despertar el display sin mover el escritorio — VERIFICADO

**Un toque de 300 ms enciende el display sin deriva medible.** Medido en tres
tandas: siete toques el 2026-08-21 y seis el 2026-08-22 dieron **cero
centímetros** de deriva, y un toque encendió un display dormido mostrando 117 cm
sin mover el escritorio.

⚠️ **Un caso quedó sin explicar.** Al refrescar tras el corte de corriente en
movimiento, el display mostró `095` y enseguida `094`. Se sospechó que el toque
movía 1 cm; **se midió y no**: seis toques seguidos, cero deriva. Lo más probable
es que el escritorio quedara en 94 y pico —se cortó bajando— y que el `095` fuera
el display asentándose tras el arranque en frío. **No se puede afirmar con lo
medido, y queda como no determinado.**

**Consecuencia práctica:** el refresco se puede usar tantas veces como haga falta
sin que el escritorio derive. Trece toques no movieron nada.

**Por qué importa:** con el display dormido los cuatro dígitos valen `0x00` y
**la altura sencillamente no está en el bus**. Sin este toque, el ESP32 no puede
saber dónde está el escritorio tras un rato de inactividad o un corte.

Implementado como el comando `w` de `desk_sniffer`. ⚠️ **Nunca sobre un canal de
memoria**: cualquier toque en M1 o M2 arranca un viaje al preset.

**Velocidad de desplazamiento: 0.68 cm/s** — VERIFICADO el 2026-08-22 sobre el
recorrido completo, igual en los dos sentidos: 44 cm en 65 s bajando y 43 cm en
64 s subiendo. Tras frenar quedan **~1 cm de inercia**, así que un control por
altura debe anticipar ese centímetro. Captura:
[capturas/2026-08-22-recorrido-completo.log](capturas/2026-08-22-recorrido-completo.log).
| Recuperar un preset (toque) | < 0.2 s | Una sola lectura de teclado |
| Toque → **movimiento continuo** | **2.2 – 2.6 s** | Medido arriba |
| Toque → **grabar preset** | **3.0 s** | Medido arriba |

**Los dos umbrales rondan los 2–3 segundos**, así que la caja parece tener un
único concepto de "pulsación larga" y aplicarlo igual a los cuatro botones.

**Diseño que sale de aquí ([ADR-023](DECISIONS.md)):** el firmware **solo emite
toques**, y un limitador de ancho de pulso por hardware corta cualquier pulso a
los **300 ms** en los cuatro canales.

```
160 ms          300 ms                      2200 ms
 |---------------|---------------------------|
 mínimo       elegido                    empieza
 del chip                                el peligro
```

Con eso, un contacto pegado **no puede arrancar movimiento continuo ni
sobrescribir un preset**: como mucho mueve un centímetro o recupera una memoria.

El chip admite además **combinaciones de KI1 y KI2 sobre el mismo DIG**, con
prioridad máxima. Si alguna función del mando usa combinación, un solo relé no
la reproduce. La captura lo dirá.

---

## Elección de GPIO

Las líneas van a **P18** (CLK, rojo) y **P4** (DIO, verde), con **GND** común.
**Ambos verificados presentes en la bornera** — ver el mapa de abajo.

**P18 se eligió para no depender de qué módulo sea esta placa**
([ADR-020](DECISIONS.md)). La asignación anterior era P16, y en los módulos
**WROVER** los GPIO 16 y 17 están cableados a la PSRAM y no sirven.

### Mapa de la bornera — VERIFICADO por fotografía

*Placa de expansión `FOR ESP32 TERMINAL ADAPTER`, leída de la fotografía del
producto con la serigrafía en horizontal y legible.*

| Columna izquierda | Columna derecha |
|---|---|
| CLK | 5V |
| SD0 | GND |
| SD1 | SD3 |
| P15 | SD2 |
| P2 | P13 |
| P0 | GND |
| **P4** ← DIO, verde | P12 |
| P16 | P14 |
| P17 | P27 |
| P5 | P26 |
| **P18** ← CLK, rojo | P25 |
| P19 | P33 |
| **GND** ← el más cómodo | P32 |
| P21 | P35 |
| RX | P34 |
| TX | SVN |
| P22 | SVP |
| P23 | EN |
| GND | 3V3 |

**Cómo encontrarlos sin depender de cómo esté orientada la placa:** desde **P4**,
contando en el sentido que aleja de P0, vienen **P16, P17, P5 y P18**. Dos
posiciones más allá de P18, pasando P19, hay un **GND** — ese es el más cómodo
para el retorno de los divisores, más que el de la esquina.

**No tocar `CLK`, `SD0`, `SD1`, `SD2` ni `SD3`.** Son los GPIO 6 a 11, cableados
a la memoria flash interna del módulo. Están sacados a la bornera pero usarlos
impide que el ESP32 arranque. Se suman a los pines de arranque (0, 2, 5, 12, 15)
en la lista de lo que no se toca.

### El módulo

**Datos leídos del propio chip por esptool el 2026-08-03**, durante una carga.
Es la mejor evidencia disponible: la reporta el silicio, no una etiqueta ni un
vendedor.

| | |
|---|---|
| Chip | **ESP32-D0WD-V3**, revisión **v3.1** |
| Núcleos | Dual core + LP core, **240 MHz** |
| Cristal | 40 MHz |
| Características | Wi-Fi, BT, calibración de Vref en eFuse, *Coding Scheme None* |
| MAC | `b4:bf:e9:0f:06:9c` |
| Flash usable | ≥ 1.310.720 bytes de programa, 327.680 de RAM |
| Core de Arduino | esp32 **3.3.11** |

*El D0WD-V3 es el chip que llevan tanto los WROOM-32E como los WROVER-E, así que
esto **no** decide por sí solo cuál es el módulo. Da igual: desde
[ADR-020](DECISIONS.md) el proyecto no depende de esa distinción.*

Serigrafía del blindaje: **`ESP-32`**, con marcado CE, `ISM 2.4G 802.11 b/g/n` y
**`FCC ID: 28B77-ESP32-32X`**. USB-serie **CP2102** de Silicon Labs.

- **Verificado:** no lleva marcado `WROVER` por ninguna parte, y el formato es el
  **corto**: el blindaje ocupa poco más de la mitad de la placa, con sitio de
  sobra debajo para el USB-C, los dos pulsadores y el regulador. Un WROVER mide
  6 mm más y no dejaría ese hueco. Es un módulo **de clase WROOM-32, sin PSRAM**.
- **Supuesto:** que sea un WROOM-32 original de Espressif. El marcado `ESP-32` y
  ese FCC ID no son los de un módulo Espressif de catálogo; es un módulo
  compatible. Para este proyecto da igual: **con CLK en P18 la distinción no
  tiene ninguna consecuencia**, y por eso se decidió así.
- Consecuencia práctica del CP2102: el puerto aparecerá como
  `/dev/cu.usbserial-XXXX`. macOS trae el driver desde Big Sur, no hay que
  instalar nada.
- Se evitan a propósito los GPIO de arranque: **0, 2, 12 y 15**. Un nivel
  inesperado en cualquiera de ellos durante el reset deja el ESP32 sin
  arrancar, o arrancando en modo de flasheo.
- El GPIO se conecta **al nodo intermedio** del divisor, nunca a un extremo.
- Todos los GND van juntos: retornos del divisor, hilo azul y GND del ESP32.
  Sin referencia común no hay medición válida.

---

## Alimentación

ESP32 por **USB desde el Mac**. El hilo rojo del escritorio no se conecta.
Ver [ADR-007](DECISIONS.md).
