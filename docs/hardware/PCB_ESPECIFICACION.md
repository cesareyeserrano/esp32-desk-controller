# Placa definitiva — especificación para fabricación

> Sustituye el montaje en protoboard, que fue **el eslabón frágil de todo el
> proyecto**: un cortocircuito entre dos hilos del bus y un optoacoplador mal
> asentado costaron dos días de diagnóstico y estuvieron a punto de dar el mando
> por muerto (bitácora del 2026-08-23).
>
> Estado: **diseño, sin fabricar.** Decisiones tomadas con el propietario el
> 2026-08-24.

---

## Qué hace esta placa

Conecta un **ESP32 DevKit** al bus y a los pulsadores de un mando de escritorio
elevable **Jiecang JK-CH506**, para leer la altura y accionar los botones.

- **Lee** el bus de dos hilos del mando (CLK/DIO), sin escribir en él nunca
- **Acciona** los cuatro pulsadores mediante contactos secos aislados
- **Aísla** el bus del ESP32 mediante un buffer alimentado por el propio mando

---

## Cambio de diseño: el buffer de aislamiento

**Es la razón principal de rehacer la placa**, no la estética.

### El problema que resuelve

En el montaje de protoboard el bus llega **directo** al GPIO a través de un
divisor. Cuando el ESP32 se queda sin alimentación con el escritorio encendido
—al cambiarlo de fuente, un corte de luz, un cable suelto—, el **diodo de
protección del GPIO conduce** y arrastra la línea del bus hacia abajo. El mando
deja de funcionar.

Ocurrió **varias veces**, y su parecido con una avería real hizo perder dos días
([ADR-019](../DECISIONS.md), [ADR-031](../DECISIONS.md)).

### La solución

Un **74HC14** (seis inversores Schmitt trigger) **alimentado desde los 5 V del
propio mando**, intercalado entre el bus y el ESP32:

```
                    ┌─────── 74HC14 ────────┐
BUS CLK ────────────┤ inv1 ──> inv2 ────────┼──[10k]──┬──> GPIO18 (ESP32)
                    │                       │         └──[18k]── GND
BUS DIO ────────────┤ inv3 ──> inv4 ────────┼──[10k]──┬──> GPIO4  (ESP32)
                    │                       │         └──[18k]── GND
                    └───────────────────────┘
                      VCC = 5 V del mando
```

**Dos inversores por línea**, no uno: el 74HC14 invierte, y dos inversiones
seguidas devuelven la señal original. Además el Schmitt trigger **limpia los
flancos**, que en un bus a 202 kHz leído por muestreo no sobra.

**Con esto, si el ESP32 muere el bus ni se entera**: el buffer sigue alimentado
por el mando y su entrada es de alta impedancia. El divisor de salida ya no
cuelga del bus, sino de la salida del buffer, que es de baja impedancia.

⚠️ **Esto usa el hilo amarillo (5 V), que hasta ahora no se tocaba.**
[ADR-007](../DECISIONS.md) lo prohibía por una razón concreta: alimentar el
**ESP32** de ahí, que pega picos de cientos de miliamperios. **Un 74HC14
consume microamperios** — menos que el propio chip del mando. La prohibición
sigue vigente para el ESP32; esta excepción es para el buffer y solo para él.

---

**Esquema dibujado, en tres láminas** — se envían al fabricante junto con este
documento:

| Lámina | Qué muestra |
|---|---|
| [1 · Sistema](plano_pcb_1_sistema.svg) | Cómo se intercala la placa entre la caja y el mando |
| [2 · Bus](plano_pcb_2_bus.svg) | Paso de los 4 hilos y derivación al buffer |
| [3 · Canales](plano_pcb_3_canales.svg) | Los cuatro optoacopladores y su conexión |

*(Un primer intento metía las tres cosas en una sola lámina y resultó
ilegible. Un esquema que hay que descifrar no sirve para fabricar.)*

## Esquema eléctrico

### Sección A — Lectura del bus (aislada)

| Componente | Valor | Conexión |
|---|---|---|
| U2 | 74HC14 (DIP-14) | VCC = JST1.4 (5 V mando) · GND = común |
| C1 | 100 nF cerámico | Entre VCC y GND de U2, **lo más cerca posible del chip** |
| R1 | 10 kΩ | Salida inv2 (U2 pin 4) → GPIO18 |
| R2 | 18 kΩ | GPIO18 → GND |
| R3 | 10 kΩ | Salida inv4 (U2 pin 8) → GPIO4 |
| R4 | 18 kΩ | GPIO4 → GND |

**Nivel resultante en el GPIO:** 5 V × 18/(10+18) = **3.21 V**. Dentro del
máximo absoluto de 3.6 V, con margen.

**Encadenado de inversores en el 74HC14:**

| Línea | Entrada | Salida intermedia | Entrada 2º inv | Salida final |
|---|---|---|---|---|
| CLK | pin 1 (1A) | pin 2 (1Y) | pin 3 (2A) | pin 4 (2Y) |
| DIO | pin 5 (3A) | pin 6 (3Y) | pin 9 (4A) | pin 8 (4Y) |

Los inversores 5 y 6 (pines 11-12-13) quedan libres: **sus entradas se atan a
GND**, nunca al aire — una entrada CMOS flotante oscila y consume.

### Sección B — Accionamiento (cuatro canales)

Idéntica para los cuatro. Con `n` = 1..4:

| Componente | Valor | Conexión |
|---|---|---|
| Rn (R5-R8) | 330 Ω | GPIO → ánodo del LED (PC817 pin 1) |
| Un (U3-U6) | PC817 (DIP-4) | pin 2 = GND · pines 3 y 4 = al pulsador |

| Canal | GPIO | Botón | Conector |
|---|---|---|---|
| 1 | 27 | Subir | JST2 pines 1-2 |
| 2 | 26 | Bajar | JST2 pines 3-4 |
| 3 | 25 | Memoria 1 | JST3 pines 1-2 |
| 4 | 33 | Memoria 2 | JST3 pines 3-4 |

**330 Ω da 6.2 mA** en el LED con el GPIO a 3.3 V y caída directa de 1.25 V. Muy
por debajo de lo que un GPIO del ESP32 entrega con holgura.

⚠️ **Las patas 3 y 4 del PC817 son el lado del mando y NO tienen polaridad
crítica en el trazado**, pero **jamás deben unirse a las patas 1 y 2**: eso
uniría los 5 V del mando con el GPIO y anularía el aislamiento, que es la razón
de ser del optoacoplador.

### Sección C — ESP32

**DevKit de 38 pines montado sobre zócalos** (dos tiras hembra de 19 vías,
paso 2.54 mm, separadas 25.4 mm entre ejes).

Pines usados: **18, 4** (bus, entrada) · **27, 26, 25, 33** (canales, salida) ·
**3V3, GND**.

⚠️ **GPIO 0, 2, 5, 12 y 15 no se usan**: son pines de strapping y su nivel en el
arranque decide cómo arranca el chip. **GPIO 6-11 tampoco**: van a la memoria
flash. Ver [ADR-020](../DECISIONS.md).

---

## Conectores

**La placa se intercala en el cable del mando**, decidido con el propietario el
2026-08-24:

```
[caja de control] ──4 hilos──> [PLACA] ──4 hilos──> [mando]
                                  ^                    │
                                  └───8 hilos──────────┘
                                    (pulsadores)
```

**Por qué así, y no como derivación:** elimina **las tres soldaduras del bus
dentro del mando**, que son exactamente las que produjeron el cortocircuito
verde–amarillo del 2026-08-23 y dos días de diagnóstico. Dentro del mando solo
quedan los ocho de los pulsadores, que no hay forma de evitar: los contactos
están ahí.

⚠️ **Contrapartida, y hay que tenerla presente: la placa pasa a estar EN SERIE.**
Como derivación, si la placa fallaba el mando seguía funcionando. Intercalada,
**una pista rota o un conector flojo dejan el escritorio sin mando**. Se mitiga:

- Trazar los cuatro hilos de paso como **pistas directas y anchas** (≥ 0.5 mm),
  sin pasar por componentes
- Tener a mano un **cable puente** que una directamente `BUS-IN` con `BUS-OUT`
  para saltarse la placa si algún día falla

**Cinco conectores, todos de 4 vías, paso 2.54 mm:**

| Ref | Nombre | 1 | 2 | 3 | 4 |
|---|---|---|---|---|---|
| **J1** | `BUS-IN` (de la caja) | CLK (rojo) | DIO (verde) | GND (azul) | +5V (amarillo) |
| **J2** | `BUS-OUT` (al mando) | CLK | DIO | GND | +5V |
| **J3** | `SUBIR/BAJAR` | Subir a | Subir b | Bajar a | Bajar b |
| **J4** | `MEM 1/2` | M1 a | M1 b | M2 a | M2 b |

**J1 y J2 van unidos pin a pin por pistas de paso.** De esas pistas salen las
derivaciones hacia el buffer (CLK, DIO) y la alimentación de U2 (+5V, GND).
**El bus no se interrumpe ni se conmuta: solo se observa.**

⚠️ **El tipo de conector de J1 y J2 depende del que use el cable original.** El
cable de fábrica tiene conector en los dos extremos, así que **no hay que cortar
nada**: se fabrican dos latiguillos cortos, o se monta en la placa el mismo tipo
de conector. **Pendiente de identificar** — ver la nota al final.

**Los pares de pulsador no tienen polaridad**: son los dos extremos de un
contacto seco. Da igual el orden dentro de cada par.

⚠️ **JST1 pin 4 son 5 V reales del mando.** No debe llegar a ningún pin del
ESP32: solo alimenta U2. Trazarlo separado del resto.

---

## Lista de materiales

| Ref | Componente | Cant. | Nota |
|---|---|---|---|
| U1 | Zócalo para ESP32 DevKit 38 pines | 2 tiras de 19 | Hembra, 2.54 mm |
| U2 | 74HC14N (DIP-14) | 1 | Con zócalo |
| U3-U6 | PC817 (DIP-4) | 4 | **Con zócalo**: uno flojo ya causó un fallo |
| R1, R3 | 10 kΩ ¼ W | 2 | Divisor, rama alta |
| R2, R4 | 18 kΩ ¼ W | 2 | Divisor, rama baja |
| R5-R8 | 330 Ω ¼ W | 4 | LED de los optoacopladores |
| C1 | 100 nF cerámico | 1 | Desacoplo de U2 |
| J1, J2 | Conector 4 vías para el bus | 2 | **Tipo pendiente**: debe encajar con el cable original |
| J3, J4 | JST-XH 4 vías, macho recto | 2 | Más conectores hembra y contactos |

---

## Notas de trazado

1. **Masa común y sólida.** ESP32, 74HC14, PC817 y el GND del bus comparten
   masa. Plano de masa continuo si la placa es de dos caras.
2. **C1 pegado a U2.** Un condensador de desacoplo lejos del chip no sirve.
3. **Separar el lado del mando.** Las pistas de JST2 y JST3 hacia las patas 3-4
   de los optoacopladores **no deben acercarse** a las pistas del ESP32: son los
   dos lados de un aislamiento galvánico. **Dejar al menos 2 mm** entre ellas, y
   no pasar una masa por debajo.
4. **El +5 V de JST1 solo llega a U2.** Ninguna pista suya debe aproximarse a
   los pines del ESP32.
5. **Serigrafía**: nombre de cada conector, número de canal junto a cada
   optoacoplador, y el pin 1 marcado en todos los circuitos integrados.
6. **Taladros de fijación** en las cuatro esquinas, 3 mm.

---

## Qué se conserva del montaje actual

Los valores del divisor **cambian** (de 16.3k/27k a 10k/18k) porque ahora
cuelgan de la salida del buffer y no del bus: ya no hay que preocuparse por
cargar el bus, y una impedancia más baja da flancos más limpios.

Todo lo demás —canales, resistencias de LED, asignación de pines— se conserva
tal cual, **verificado en funcionamiento durante días**.

---

## Antes de encargarla

⚠️ **El buffer no se ha probado nunca.** Todo lo demás de esta placa lleva días
funcionando, pero la sección A es diseño nuevo. **Conviene montarla primero en
protoboard** y comprobar dos cosas antes de gastar dinero en una PCB:

1. Que el sniffer sigue leyendo el bus con la misma calidad — comparable con la
   referencia medida: **0.67% de transacciones malformadas**
2. Que **desconectando el ESP32, el mando sigue funcionando** — que es
   literalmente para lo que se añade

### Dato pendiente: el conector del cable original

Para especificar J1 y J2 hace falta identificar el conector del cable de 4 hilos
que va de la caja al mando. Lo que hay que mirar:

- **Distancia entre pines** (paso): 2.0 mm y 2.54 mm son los habituales
- **Forma del cuerpo**: si lleva pestaña de retención y de qué lado
- **Marca en el plástico**, si la hay: `XH`, `PH`, `ZH`, `SM`...

Con eso se pide el conector correcto. Si resulta ser un JST-XH de 2.54 mm —lo
más probable en este tipo de mandos— **los cuatro conectores de la placa serían
del mismo tipo**, y basta con un cable de 4 hilos con conector hembra en ambos
extremos para el tramo placa→mando.
