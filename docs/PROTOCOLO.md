# Protocolo del bus del mando

> **Estado: DESCIFRADO.** Verificado contra el datasheet y contra capturas
> reales del bus del escritorio.
>
> Fuente: *AiP650E Product Specification*, Wuxi I-CORE, doc
> `AiP650E-AX-XS-B037EN`, versión 2024-01-B1. Todo lo marcado **[datasheet]**
> sale de ahí. **[medido]** es verificación propia sobre la placa.
> **[capturado]** sale de las capturas del 2026-08-06 en
> [capturas/](capturas/). Lo que no lleve marca es interpretación, y lo dice.

---

## Cómo se lee la altura

**[capturado]** — verificado contra lo que muestra la pantalla del mando.

La caja de control repite un ciclo cada **~200 ms**, siempre el mismo:

```
S 68 <seg> P     escribe el digito 1
S 6A <seg> P     escribe el digito 2
S 6C <seg> P     escribe el digito 3
S 6E <seg> P     escribe el digito 4
S 4F <tecla> P   lee el teclado
```

Los cuatro bytes de segmentos, traducidos con la tabla de segmentos, **son
literalmente el número que se ve en pantalla**: la altura en centímetros
enteros, con el cuarto dígito en blanco. `080` = 80 cm, `117` = 117 cm.

**Verificación cruzada del 2026-08-06**, comparando lo leído del bus con lo que
la persona veía en el mando en ese mismo momento:

| Acción | Pantalla | Bus |
|---|---|---|
| Subir manual | 86 | `086` ✅ |
| Bajar manual | 77 | `077` ✅ |
| Ir a memoria alta | 117 | `117` ✅ |
| Ir a memoria baja | 80 | `080` ✅ |

El caso de **117 es el que importa**: confirma que el dígito de las centenas
también decodifica bien. Ninguna captura previa había pasado de 99.

### La altura se refresca DURANTE el movimiento ✅

**[capturado]** — y era el riesgo que podía tumbar el proyecto entero.

Mientras el escritorio se mueve, la altura se actualiza **cada centímetro**, sin
esperar a llegar. Medido: **~1.2 s por centímetro**, o sea **8.5 mm/s**, idéntico
subiendo y bajando (39 cm en 45.9 s de subida, 36 cm en 42.5 s de bajada).

En los primeros centímetros tras arrancar tarda más —2.5 a 3 s cada uno— antes
de estabilizarse. Es la rampa de aceleración del motor, y es exactamente la razón
por la que [ADR-001](DECISIONS.md) descartó contar segundos.

**Consecuencia: el lazo cerrado es viable.** El requisito real del proyecto queda
confirmado sobre hardware.

### Al pulsar una memoria, el display anuncia el destino primero

**[capturado]** — hallazgo no buscado, y útil.

Al pulsar un botón de memoria, la pantalla **parpadea la altura de destino**
antes de empezar a moverse, y solo después pasa a contar la altura real:

```
102.47   '  7'
102.67   '117'   <- parpadea el DESTINO, tres veces
103.87   '117'
104.07   '078'   <- y ahora cuenta desde donde esta
104.47   '079'
  ...
150.00   '117'   <- llega
```

Eso permite que el ESP32 sepa **a dónde va el escritorio en el instante en que
alguien pulsa memoria**, no solo dónde está. Para la fase 4 significa poder
anticipar el movimiento en vez de perseguirlo.

Distinguirlo de una altura real es directo: el destino aparece **antes** de que
la altura empiece a cambiar, y alterna con dígitos en blanco.

### Rango real y comportamiento en los topes

**[capturado]** — [2026-08-06-topes-fisicos.log](capturas/2026-08-06-topes-fisicos.log),
recorriendo todo el rango en los dos sentidos.

**El escritorio va de 73 a 118 cm.** 45 cm de recorrido.

**Al llegar al tope no pasa nada especial**, y eso es una buena noticia:

- **El display sigue mostrando un número.** No aparece código de error ni ningún
  patrón de segmentos que el decodificador no sepa traducir — verificado: cero
  códigos desconocidos en toda la captura.
- **No parpadea al topar.**
- **No aparece ningún comando nuevo** en el bus.
- La cadencia se mantiene en ~1.2 s por centímetro hasta el final; el último
  centímetro de arriba tardó 1.4 s. Apenas frena.

**Consecuencia para el firmware:** llegar al tope es indistinguible de estar
parado. La única señal es que **la altura deja de cambiar** aunque se sigan
mandando toques. Ese es el criterio que habrá que usar para no encadenar toques
indefinidamente contra un tope.

### Qué significa el parpadeo del display

**[capturado]** — corregido respecto a una interpretación anterior.

El display **parpadea cuando arranca un movimiento continuo**, no cuando llega al
tope. Confirmado en dos capturas: 0 parpadeos tras un toque de 2.2 s que se paró
solo, 2 y 3 tras las pulsaciones que dispararon movimiento continuo, y **ninguno
al topar**.

Sirve para que el firmware detecte que se disparó un movimiento que no pidió.

### Control del display

**[capturado]** — 17 apariciones a lo largo de todas las capturas.

El comando `0x48` va siempre seguido del **mismo byte, `0x21`**:

```
S 48 21 P   ->   display=ON, sleep=no, seg=8, brightness=2
```

**Nunca se ha visto la caja mandar el bit de sueño.** Con el escritorio quieto el
display se pone en blanco escribiendo `0x00` en los cuatro dígitos, pero el chip
**no se duerme** y el bus sigue refrescando cada 200 ms. Eso mantiene abierta la
pregunta 6 —si tras mucho más tiempo llega a dormirse— pero descarta que lo haga
en los minutos que llevan las capturas.

### Comando `0x90` — SIN IDENTIFICAR

**[capturado]** — y es lo único del protocolo que sigue sin explicación.

Aparece en **las cinco capturas**, entre una y cinco veces cada una, siempre con
el mismo formato de dos bytes y siempre con ACK:

```
S 90 42 P   ->   desconocido
```

Se cuela **al principio de un ciclo de refresco**, en la posición donde debería
ir el `0x68` del primer dígito, y el `0x68` llega ~300 µs después.

No encaja con ningún comando del datasheet. Es raro —una vez cada dos o tres
minutos— y **no impide leer la altura**, así que no bloquea nada. Pero está sin
explicar y conviene no fingir lo contrario.

### Códigos de tecla

**[capturado]** — mapeados el 2026-08-06 pulsando cada botón por separado.

| Código | Botón | Cómo se identificó |
|---|---|---|
| **`0x47`** | **Subir** | 15 lecturas seguidas mientras subía |
| **`0x57`** | **Bajar** | 18 lecturas seguidas mientras bajaba |
| **`0x6F`** | **Memoria** (la de 117 cm) | Una lectura, justo antes de ir a 117 |
| **`0x67`** | **Memoria** (la de 80 cm) | Una lectura, justo antes de ir a 80 |
| — | **Reset** | **Sin capturar y sin capturar nunca.** [ADR-008](DECISIONS.md) |

Todos llevan el **bit 6 a uno** cuando la tecla está pulsada, como dice el
datasheet — lo que confirma desde el hardware real el razonamiento de
[ADR-011](DECISIONS.md): simular una pulsación exigiría forzar ese bit a uno en
un bus que solo puede tirar hacia abajo.

**En reposo** se ven `0x07`, `0x17`, `0x21`, `0x27` y `0x2F`, siendo `0x27` el
dominante con diferencia. **El datasheet dice que sin tecla vale `0x2E`; en esta
placa no es así.** Diferencia real, anotada. Lo que sí se cumple sin excepción es
que el bit 6 está a cero en todos los valores de reposo, y ese es el bit que
importa para detectar una pulsación.

### Tiempos de pulsación, medidos **[capturado]**

| | Duración | Cómo se midió |
|---|---|---|
| **Recuperar** un preset (toque) | **< 0.2 s** | Produce una sola lectura de teclado |
| **Grabar** un preset (mantener) | **3.0 s** | 15 lecturas seguidas antes del primer parpadeo |

La caja **confirma la grabación con cinco parpadeos del display cada 0.6 s**, que
siguen aunque se suelte el botón. Detalle y captura en
[HARDWARE.md](HARDWARE.md).

Quince veces de margen entre recuperar y grabar. Es el número que le faltaba a
[ADR-010](DECISIONS.md) y que bloqueaba el cableado de las memorias.

---

## El chip

**AiP650EO**, marcado `19BT450` (código de lote), SOP-16, paso 1.27 mm.

Es la variante SOP16 del **AiP650E** de Wuxi I-CORE. Driver de LED de
**8 segmentos × 4 dígitos, cátodo común**, con escaneo de teclado **7×4**
integrado y soporte de algunas combinaciones. Compatible a nivel de software
con el TM1650 de Titan Micro.

| Parámetro | Valor | Fuente |
|---|---|---|
| Alimentación | 3 – 5.5 V (típico 5 V) | [datasheet] |
| Corriente en reposo | 0.3 mA típico | [datasheet] |
| Corriente en sueño | 0.05 mA típico | [datasheet] |
| Velocidad del bus | 0 a 4 Mbps | [datasheet] |
| Ciclo de refresco de display | 4 – 20 ms, típico 8 ms | [datasheet] |
| Intervalo de escaneo de teclado | 20 – 80 ms, típico 40 ms | [datasheet] |

### Pinout SOP-16 [datasheet] + [medido]

| Pin | Nombre | Función | Hilo del cable |
|---|---|---|---|
| 1 | DIG1 | Dígito 1 / fila 1 del teclado | |
| **2** | **CLK** | Reloj, entrada. **Pull-up interno** | **Rojo** |
| **3** | **DIO** | Datos, bidireccional. **Open-drain, pull-up interno** | **Verde** |
| **4** | **GND** | Tierra | **Azul** |
| 5 | DIG2 | Dígito 2 / fila 2 | |
| 6 | DIG3 | Dígito 3 / fila 3 | |
| 7 | DIG4 | Dígito 4 / fila 4 | |
| 8 | A/KI1 | Segmento A / columna 1. Pull-down interno | |
| 9 | B/KI2 | Segmento B / columna 2. Pull-down interno | |
| **10** | **VDD** | Alimentación | **Amarillo** |
| 11 | C/KI3 | Segmento C / columna 3 | |
| 12 | D/KI4 | Segmento D / columna 4 | |
| 13 | E/KI5 | Segmento E / columna 5 | |
| 14 | F/KI6 | Segmento F / columna 6 | |
| 15 | G/KI7 | Segmento G / columna 7 | |
| 16 | DP/KP | Punto decimal | |

**Medido el 2026-08-02:** los cuatro hilos del cable dan **0.2 Ω** contra las
patas 2, 3, 4 y 10. Coincidencia exacta con el datasheet.

De paso, esos 0.2 Ω dicen otra cosa: **esta placa no lleva las resistencias de
220 Ω en serie** que el circuito de aplicación recomendado pone entre el
conector externo y las patas CLK/DIO. La conexión es directa.

### Niveles eléctricos [datasheet]

| Parámetro | Valor |
|---|---|
| CLK/DIO nivel bajo (VIL) | máx. 0.2 × VDD = **1.0 V** |
| CLK/DIO nivel alto (VIH) | mín. 0.7 × VDD = **3.5 V** |
| Pull-up interno de CLK (IUP1) | 550 µA típico ≈ **9.1 kΩ** a 5 V |
| Pull-up interno de DIO (IUP2) | 550 µA típico ≈ **9.1 kΩ** a 5 V |
| Máximo absoluto en cualquier entrada | VDD + 0.5 V |

**Este es el dato que desbloqueó el montaje.** El pull-up del bus no hay que
medirlo: viene dentro del chip y el datasheet lo especifica. Si además hubiera
resistencias de pull-up externas —el circuito recomendado lleva dos de 10 kΩ—
el pull-up resultante solo sería más fuerte, nunca más débil. Dimensionar la
sonda para 9.1 kΩ es seguro pase lo que pase. Ver [ADR-013](DECISIONS.md).

---

## Quién manda en el bus

La caja de control es el **master**. El AiP650E es **esclavo**: no decide nada,
solo obedece y responde. Toda la lógica de comportamiento —qué es un pulso
corto, cuántos milisegundos son "mantenido", cuándo eso significa grabar un
preset— vive en la caja de control. El mando es un periférico tonto.

Por el cable viajan dos clases de transacción: escrituras de display (la caja le
dice al chip qué segmentos encender) y lecturas de teclado (la caja le pregunta
qué tecla está pulsada).

## Trama [datasheet]

Parecido a I2C, con START, STOP y ACK, MSB primero. **Sin direccionamiento de
7 bits**: el primer byte tras el START es un comando fijo, no una dirección más
bit R/W. Ver [ADR-006](DECISIONS.md).

- START: CLK en alto y DIO pasando de alto a bajo.
- STOP: CLK en alto y DIO pasando de bajo a alto.
- Los datos se enganchan en el **flanco de subida de CLK**. DIO no puede cambiar
  con CLK en alto.
- ACK: en el noveno pulso de CLK. En una lectura de teclado, el ACK del byte de
  comando es 0 y el del byte de datos es 1.

**Regla útil para el decodificador:** el **bit 0 del byte de comando distingue
escritura de lectura**. `0x48` (sistema) y `0x68`/`0x6A`/`0x6C`/`0x6E`
(dígitos) terminan en 0 y son escrituras; `0x49` (leer tecla) termina en 1.

---

## Comandos [datasheet]

| Byte | Nombre | Qué hace |
|---|---|---|
| `0x48` | System Instruction | Fija parámetros de sistema. Le sigue el byte de instrucción de display |
| `0x68` | Dirección DIG1 | Le sigue el byte de segmentos del dígito 1 |
| `0x6A` | Dirección DIG2 | Ídem dígito 2 |
| `0x6C` | Dirección DIG3 | Ídem dígito 3 |
| `0x6E` | Dirección DIG4 | Ídem dígito 4 |
| `0x49` | Get key | Lee el teclado. El chip responde un byte |

El comando `Get key` está definido como `0100_1XX1`, con los bits 2 y 1
indiferentes. Así que **también valen `0x4B`, `0x4D` y `0x4F`** — el
decodificador debe enmascarar esos dos bits en vez de comparar contra `0x49`.

Al encender, primero se transfieren los datos a RAM y después se enciende el
display.

### Byte de instrucción de display [datasheet]

El byte que sigue a `0x48`:

| Bit | Nombre | Significado |
|---|---|---|
| 6–4 | BR[2:0] | Brillo. `000` = 8 niveles, `001` = 1 nivel … `111` = 7 niveles |
| 3 | S | `1` = modo 7 segmentos, `0` = modo 8 segmentos |
| **2** | **W** | **`1` = modo sueño activado**, `0` = desactivado |
| 0 | D | `1` = display encendido, `0` = apagado |

Los bits 2 y 0 son los que importan para [ADR-012](DECISIONS.md): la caja de
control **puede apagar el display y puede dormir el chip**. Si lo hace, el
refresco se detiene y la altura leída se queda congelada.

## Segmentos [datasheet]

| Segmento | A | B | C | D | E | F | G | DP |
|---|---|---|---|---|---|---|---|---|
| Bit | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |

Tabla de dígitos, que es todo lo que hace falta para leer la altura:

| Dígito | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
|---|---|---|---|---|---|---|---|---|---|---|
| Byte | `0x3F` | `0x06` | `0x5B` | `0x4F` | `0x66` | `0x6D` | `0x7D` | `0x07` | `0x7F` | `0x6F` |

Con el bit 7 puesto, el dígito lleva punto decimal: `74.5` en pantalla son los
bytes `0x07`, `0xE6`, `0x6D`.

**El decodificador de altura se puede escribir antes de capturar nada.** La
captura confirma; no descubre.

## Teclado [datasheet]

Ante el comando `Get key`, el chip responde un byte. Tabla oficial:

| | DIG1 | DIG2 | DIG3 | DIG4 |
|---|---|---|---|---|
| **SIN TECLA** | `00_101_110` = **`0x2E`** | ← el mismo para las cuatro | | |
| KI1 | `01_000_100` `0x44` | `01_000_101` `0x45` | `01_000_110` `0x46` | `01_000_111` `0x47` |
| KI2 | `0x4C` | `0x4D` | `0x4E` | `0x4F` |
| KI3 | `0x54` | `0x55` | `0x56` | `0x57` |
| KI4 | `0x5C` | `0x5D` | `0x5E` | `0x5F` |
| KI5 | `0x64` | `0x65` | `0x66` | `0x67` |
| KI6 | `0x6C` | `0x6D` | `0x6E` | `0x6F` |
| KI7 | `0x74` | `0x75` | `0x76` | `0x77` |
| KI1+KI2 | `0x7C` | `0x7D` | `0x7E` | `0x7F` |

Estructura: **bit 6 = tecla pulsada**, bits 5–3 = columna KI, bit 2 = siempre 1,
bits 1–0 = fila DIG.

Reglas de comportamiento [datasheet]:

- **Una pulsación solo se reconoce si dura al menos dos periodos de escaneo.**
  Con el intervalo entre 20 y 80 ms, el suelo real es de **160 ms** en el peor
  caso. Determina la duración mínima de un pulso de relé — ver
  [HARDWARE.md](HARDWARE.md).
- Las combinaciones KI1+KI2 sobre el mismo DIG tienen **prioridad máxima**.
- Si hay varias teclas a la vez, gana **el código más bajo**.

### Por qué esto cierra la puerta a la inyección — [datasheet]

Sin tecla, el chip devuelve `0x2E` = `0010_1110`. Con tecla, por ejemplo
`0x44` = `0100_0100`.

Para fabricar una pulsación habría que llevar el **bit 6 de 0 a 1**. En un bus
open-drain solo se puede forzar un bit a **0**; el 1 lo pone el pull-up y nadie
más. No es difícil: es imposible. Ver [ADR-011](DECISIONS.md).

**Lo único que sí se puede hacer, y no sirve:** forzando bits a 0 se puede
enmascarar una pulsación real, o convertir una tecla en otra de código menor —
`0x74` (KI7) se puede degradar a `0x44` (KI1). Ambas cosas exigen que alguien
esté pulsando físicamente, así que no valen para automatizar.

**Prohibido** forzar DIO a alto con una salida push-pull: mientras el AiP650E
tira de la línea abajo, eso es un cortocircuito contra la salida del chip.

---

## Matriz de teclado y montaje del mando [datasheet]

Los botones cierran una columna **KI** contra una fila **DIG**. Las KI llevan
pull-down interno (30–90 µA) y las DIG son salidas. El circuito de aplicación
recomienda **2 kΩ en serie en cada línea DIG** dentro de la matriz de teclas,
para que las pulsaciones no perturben el display.

Esto explica la medida de **5 V entre las patas de un pulsador**: la línea DIG
está en alto y la KI en bajo por su pull-down. Ver
[ADR-004](DECISIONS.md).

Para el accionamiento por relé, el contacto del relé va **en paralelo al
pulsador**, haciendo exactamente lo mismo que el botón.

---

## Preguntas abiertas

### Respondidas por las capturas del 2026-08-06 ✅

1. ~~**¿Qué dígitos usa y en qué orden?**~~ → `68`, `6A`, `6C`, `6E` en ese
   orden, de izquierda a derecha. El cuarto va siempre en blanco. Sin punto
   decimal: la altura son centímetros enteros.
2. ~~**¿A qué velocidad corre el bus?**~~ → **~202 kHz**, y esa cifra es un
   suelo. Obligó a rehacer la captura: por interrupción no daba abasto, y se
   pasó a muestreo por ráfagas.
3. ~~**¿Cada cuánto refresca la altura?**~~ → ciclo completo cada **~200 ms**;
   la altura cambia cada **~1.2 s**, que es lo que tarda en moverse 1 cm.
4. ~~**¿Qué código de tecla es cada botón?**~~ → subir `0x47`, bajar `0x57`,
   memorias `0x6F` y `0x67`. Tabla arriba.
5. ~~**¿Se refresca la altura durante el movimiento?**~~ → **Sí.** Cada
   centímetro, sin esperar a llegar. **El lazo cerrado es viable.**

### Todavía abiertas

6. ~~**¿Apaga el display o duerme el chip por inactividad?**~~ → **Apaga el
   display, pero NO duerme el chip.** Medido con el escritorio quieto durante
   **15 minutos**: el sniffer se armó **4505 veces y ninguna encontró el bus en
   silencio**. A los pocos segundos la pantalla se apaga escribiendo `0x00` en
   los cuatro dígitos, pero el refresco de 200 ms no se detiene nunca, y el
   comando de control **siempre dice `sleep=no`**.
   **Consecuencia:** el ESP32 puede distinguir *"no hay altura que mostrar"* de
   *"no hay bus"*, que era justo lo que [ADR-012](DECISIONS.md) necesitaba.
   *Sin verificar: qué pasa tras horas, o al volver de un corte de corriente.*
9. **¿Qué es el comando `0x90 42`?** Aparece en las cinco capturas, una a cinco
   veces cada una, y no está en el datasheet. Ver arriba. No bloquea nada.
7. **¿Alguna función usa combinación de teclas?** El chip lo soporta. Si grabar
   preset o el reset fueran combinaciones, un solo relé no las reproduce. Nada
   en las capturas lo sugiere todavía, pero no se ha probado a propósito.
8. ~~**¿Cuánto hay que mantener M1/M2 hasta que graba?**~~ → **3.0 s**, medido
   el 2026-08-06. Ver arriba. Respondida.

---

## Tabla de confirmación propia

Lo observado en el bus real, que en algún punto **difiere del datasheet**.

| Byte observado | Interpretación | Captura |
|---|---|---|
| `68` `6A` `6C` `6E` | Direcciones de los cuatro dígitos, de izquierda a derecha | ambas del 2026-08-06 |
| `4F` | Comando de lectura de teclado. Encaja con la máscara `0100_1XX1` | ambas |
| `0x27` | Teclado en reposo, valor dominante. **El datasheet dice `0x2E`** | ambas |
| `0x07` `0x17` `0x21` `0x2F` | Otros valores de reposo, todos con el bit 6 a cero | ambas |
| `0x47` | Tecla **subir** pulsada | [pulsadores](capturas/2026-08-06-pulsadores.log) |
| `0x57` | Tecla **bajar** pulsada | pulsadores |
| `0x6F` | Tecla de **memoria** (preset de 117 cm) | pulsadores |
| `0x67` | Tecla de **memoria** (preset de 80 cm) | pulsadores |
| `0x00` en los 4 dígitos | Display en blanco, con el bus aún refrescando | ambas |

<details>
<summary>Tabla original, previa a las capturas</summary>

| Byte observado | Interpretación | Captura |
|---|---|---|
| | | |

*Quedó vacía: se llenó de una vez con las dos capturas del 2026-08-06.*

</details>

---

> **Correcciones registradas.** Una versión anterior de este archivo daba el
> pinout como 5 = SCL, 6 = SDA, 15 = GND, 16 = VDD, tomado de components101.
> **Estaba mal**, y la medición lo desmintió. Otra versión advertía que los
> comandos y el formato de teclado procedían de fuentes del TM1650 y no del chip
> real; esa advertencia ya no aplica: el datasheet de I-CORE los confirma uno
> por uno. Detalle en la [bitácora](BITACORA.md).
