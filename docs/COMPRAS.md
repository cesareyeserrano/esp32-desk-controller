# Lista de compras

> Qué hace falta y **por qué**. Un artículo sin justificación no entra: dentro de
> seis meses nadie recuerda para qué se pidió.

---

# 🛒 LISTA ACUMULADA — lo que falta comprar

**Nada de esto bloquea el trabajo actual.** Se acumula aquí a propósito, para
hacer **un solo pedido** en vez de varios de días de espera cada uno. Cuando
aparezca algo nuevo, se añade a esta tabla.

*Última actualización: 2026-08-06.*

| Cant. | Artículo | Para qué | ¿Bloquea? |
|---|---|---|---|
| 2 | **CD4538** o **74HC123** (monoestable doble) | Limitador de pulso independiente del ESP32. Dos canales por chip, cuatro canales en total | No — [ADR-024](DECISIONS.md) lo aplaza; de momento lo hace el watchdog |
| 4 | **Condensador ~1 µF** | Fija el tiempo del monoestable junto a su resistencia | No |
| 4 | **Resistencia ~300 kΩ** | Ídem. El valor exacto se calcula contra el datasheet del chip que se compre | No |
| 5 m | Cable **AWG 30** o **AWG 28** | Ocho derivaciones más, a los cuatro pulsadores del mando | **Sí, para la fase 3** — comprobar si queda del anterior |
| 1 | **Placa perforada** para montaje definitivo | Pasar la sonda y el accionamiento de protoboard a algo permanente | No, es el final de la fase 3 |
| 1 | ESP32 DevKit de repuesto, 38 pines | Si se estropea el único, el proyecto se para semanas. WROOM o WROVER, da igual | No |
| 1 | Analizador lógico USB 8 canales | **Decidido que no** el 2026-08-03. Se compraría solo si una captura sale sucia y no se puede diagnosticar. Anotado por si cambia | No |

**Lo único con dependencia real es el cable fino.** Si no quedó del pedido
anterior, hay que conseguirlo antes de cablear los pulsadores — y los hilos de un
cable USB viejo cortado sirven, como ya se hizo con la sonda.

**Sobre el monoestable:** las condiciones que obligarían a montarlo están en
[ADR-024](DECISIONS.md) — que el sistema funcione habitualmente sin nadie
delante, que se observe un solo caso de watchdog que no dispare, o que se cablee
algo con contacto mecánico en vez de PC817.

---

## Lo ya comprado y en casa

Resistencias de 27 kΩ, 10 kΩ y 330 Ω. Optoacopladores **PC817 sueltos en DIP-4**,
comprobados el 2026-08-06: ninguna continuidad entre sus cuatro patas, que es lo
correcto — el LED no pita por su caída de ~1.2 V y el fototransistor está abierto
en reposo. **Al ser chips sueltos no llevan jumper ni circuitería que
neutralizar**, así que las dos comprobaciones que quedaban pendientes sobre los
módulos de dos canales ya no aplican.

---

Estado a 2026-08-03. **El proyecto se compra en Bogotá, Colombia.**

**Lo único que hace falta ahora mismo son dos resistencias de 27 kΩ**, que en
cualquier tienda de electrónica cuestan unos cientos de pesos. Todo lo demás de
esta lista es opcional o de fases posteriores.

Orden de magnitud del pedido completo: **$220.000 – $260.000 COP**; recorte
mínimo viable (surtido de resistencias, analizador lógico y photoMOS), unos
**$110.000 COP**.

> **Sobre los precios.** Las cifras en euros de las tablas de abajo son precios
> de referencia europeos/asiáticos, convertidos a ~3.600 COP/€ (tasa de agosto de
> 2026, que no es oficial en Colombia y varía según el proveedor). **No son
> precios locales verificados.** En Colombia lo importado suele salir más caro,
> así que trata los pesos como suelo, no como techo. Confirmar en tienda.

## Dónde comprar en Bogotá

Para dos resistencias no vale la pena pedir nada por internet: se va a una tienda
y se sale con ellas. Tiendas colombianas que manejan este tipo de componentes:

| Tienda | Notas |
|---|---|
| [Sigma Electrónica](https://www.sigmaelectronica.net/) | Cra. 24 No. 61D-65, Bogotá. Maneja ESP32 y componentes |
| [Vistrónica](https://www.vistronica.com/) | Bodega en Bogotá (barrio Mandalay), L-V 8:00–17:00, sábado hasta mediodía |
| [Didácticas Electrónicas I+D](https://www.didacticaselectronicas.com/) | Componentes y material educativo |
| [Electronilab](https://electronilab.co/) | Envíos a toda Colombia |
| [DynamoElectronics](https://www.dynamoelectronics.com/) | Arduino, ESP32, robótica |
| [MercadoLibre Colombia](https://www.mercadolibre.com.co/) | Para lo que no esté en las anteriores |

**Confirmar dirección y horario antes de desplazarse** — no están verificados en
sitio, salen de la web de cada tienda.

Lo más difícil de conseguir localmente son los **photoMOS** (AQY212GS, TLP222A).
Si no aparecen, [LCSC](https://www.lcsc.com/) los tiene, pero con envío desde
China y la espera que eso implica. Conviene buscarlos con tiempo, aunque son de
fase 3 y no bloquean nada ahora.

---

## Resistencias — desbloquea la fase 2

**Diez resistencias en total** para llegar hasta mover el escritorio. Dos de
ellas ahora, ocho en fase 3.

| Cant. | Valor | Cuándo | Para qué |
|---|---|---|---|
| **2** | **27 kΩ** | **Ahora** | Abajo del divisor de la sonda, una por línea ([ADR-016](DECISIONS.md)). Sin esto no hay captura |
| 4 | 10 kΩ | Fase 3 | Pull-downs de los cuatro GPIO de control ([ADR-010](DECISIONS.md)) |
| 4 | 330 Ω | Fase 3 | LED de entrada de los cuatro optoacopladores. Ver nota abajo |
| — | 22 kΩ o 33 kΩ | Ahora, si no hay de 27 kΩ | Sustituyen a las de 27 kΩ, no se suman |

*Las 9.1 kΩ que van arriba del divisor **ya están en el cajón**, medidas. Por eso
la compra de la fase 2 son dos piezas y no cuatro.*

**Sobre los 330 Ω.** Con un GPIO a 3.3 V y una caída directa del LED de ~1.25 V,
330 Ω dan **6.2 mA**, muy por debajo de lo que un GPIO del ESP32 entrega sin
despeinarse y de sobra para cualquier optoacoplador. **El valor definitivo se
confirma contra el datasheet de la pieza que se acabe comprando.**

Si el vendedor tiene un **surtido E12 de 1/4 W**, sale más barato que pedir
valores sueltos y viene etiquetado — lo que elimina de raíz los errores de
lectura de color que ya anularon un diseño ([ADR-015](DECISIONS.md)).
Preferir película metálica al 1%; **guardarlas en sus bolsas**, porque las de 1%
llevan cinco bandas y son aún más difíciles de leer.

---

## Quita riesgo real

### Analizador lógico USB, 8 canales, 24 MHz — **NO se compra todavía**

**Decidido el 2026-08-03: fuera del pedido.** No es necesario. El proyecto entero
está diseñado alrededor de la restricción de no tenerlo, y el sniffer ya trae
incorporado lo que haría falta: contador de transacciones malformadas, contador
de flancos perdidos y medida del reloj más rápido visto. Esas tres cosas
responden las mismas preguntas, más despacio.

Además, lo más probable es que no haga falta nunca: esta familia de chips suele
correr a decenas de kHz, muy dentro de lo que el ESP32 captura sin despeinarse.

**Cuándo sí comprarlo, y solo entonces:** si la captura sale sucia —`malformed`
sostenido, o flancos perdidos con el volcado crudo apagado— y no se puede
distinguir si la culpa es del divisor redondeando flancos, del bus yendo más
rápido de lo esperado, o del decodificador. Ese es un problema difícil de
depurar a ciegas, y ahí los ~$50.000 se pagan solos en una tarde.

Hasta que ese momento llegue, es comprar una herramienta para un problema que
todavía no existe.

*Esto cierra la decisión pendiente que [PLAN.md](PLAN.md) tenía anotada sobre si
eliminar la restricción de partida del proyecto. Respuesta: no, se mantiene.*

<details>
<summary>Para qué serviría, si algún día se compra</summary>

Funciona con **PulseView / sigrok** (gratuito, macOS incluido) y decodifica I2C
de fábrica. Responde en treinta segundos preguntas que al ESP32 le cuestan una
sesión: a qué velocidad corre el bus, si los flancos llegan limpios a través del
divisor, y si un byte raro es del bus o del decodificador.

No sustituiría al ESP32 —ese tiene que quedarse puesto de forma permanente.

</details>

**Conectarlo al mismo nodo del divisor que ya alimenta al ESP32** — literalmente
el mismo punto, en paralelo con la entrada del ESP32. Los modelos baratos no
siempre toleran 5 V en la entrada, y su impedancia de entrada es tan alta que no
perturba nada.

**Lo que no se puede hacer es montarle un segundo divisor de los mismos
valores.** Dos divisores de 36.1 kΩ en paralelo cargan el bus con 18 kΩ, y el
nivel alto se hunde a **3.32 V**, por debajo del VIH de 3.5 V del chip: el mando
empezaría a fallar de forma intermitente, y el síntoma apuntaría al analizador
antes que a la carga. Un nodo, dos instrumentos colgados de él.

| Artículo | Por qué | ~€ |
|---|---|---|
| Analizador lógico USB 8 canales 24 MHz | Ver arriba | 8–12 |
| **2 × módulo adaptador de nivel BSS138**, 4 canales | Plan B por si el bus resulta rápido y el divisor redondea demasiado los flancos ([ADR-018](DECISIONS.md); el plan B se declaró en ADR-013 y se re-ancló ahí). **Sustituye al divisor, no se suma a él** — su resistencia de 10 kΩ va del bus a 5 V, así que refuerza el pull-up en vez de cargar la línea. La otra opción válida es un buffer 74LVC2G17 | 4 |
| **ESP32 DevKit de repuesto**, 38 pines | Si se estropea el único, el proyecto se para semanas. **WROOM o WROVER, da igual**: desde [ADR-020](DECISIONS.md) el sniffer usa GPIO18 y GPIO4, libres en los dos | 6 |

---

## Fase 3 — accionamiento

**Los photoMOS quedan descartados por precio** — [ADR-021](DECISIONS.md). Un
G3VM-61A1 cuesta ~$120.000 COP en Colombia; cuatro son ~$480.000, más que todo el
resto del proyecto. ADR-017 los eligió justificándose en que "cuestan lo mismo
que un café", cosa cierta en Europa y falsa aquí.

**Lo que se compra en su lugar, por orden:**

| Opción | Coste | Estado |
|---|---|---|
| **8 × PC817** (chip suelto, DIP-4) o 2 × módulo PC817 de 2 canales | Miles de pesos | **Primera opción.** Pruebas pendientes, ver arriba |
| Los **relés mecánicos del inventario** | $0 | Segunda opción. Pueden volverse intermitentes; el fallo es benigno |
| **Relé reed** de 5 V | Por consultar | Tercera. Sellado, resuelve el problema de raíz y es barato |
| photoMOS | ~$120.000 c/u | **Solo si todo lo demás falla de verdad** |

*Referencias verificadas el 2026-08-03 por si algún día hicieran falta:
**G3VM-61A1** (Omron) y **TLP222A** (Toshiba) son DIP-4 con patas y sirven;
**AQY212GS** (Panasonic) es SOP-4 de montaje superficial y **no entra en
protoboard** — ADR-017 lo proponía el primero sin mirar el encapsulado.*

### Candidato más barato y local: PC817 — sin decidir, sin verificar

Los photoMOS son lo difícil de conseguir en Bogotá. El **PC817** es un
optoacoplador corriente que hay en cualquier tienda y cuesta una fracción.
Evaluado el 2026-08-03 sobre fotografías y ficha de vendedor, **no sobre el
mando real**.

**Los números salen con muchísimo margen.** El contacto tiene que dejar pasar
los 30–90 µA del pull-down interno de la línea KI:

| | Valor | Contra lo que hace falta |
|---|---|---|
| Fuga con el LED apagado (I_CEO) | ≤ 0.1 µA | 0.1–0.3 % del pull-down. No finge una tecla |
| Corriente que puede pasar encendido | ~0.7–2 mA con solo 0.7 mA de LED, a CTR 200 % del PC817**C** | 8–20 veces lo necesario |
| Caída al conducir | Muy por debajo de 0.1 V a 90 µA | Despreciable frente a los 5 V de la matriz |

Consecuencia útil: **la carga es tan pequeña que da igual que el GPIO del ESP32
dé 3.3 V** y la ficha del módulo pida 3.6 V mínimo. Esa cifra importa para una
carga normal; aquí sobra corriente por un factor de diez.

**Lo que hay que hacer al módulo de 2 canales:** su jumper amarillo selecciona
salida con *pull-up* o con *pull-down*. Cualquiera de las dos inyectaría
corriente permanente en la matriz del teclado y el chip podría ver una tecla
apretada. **El jumper se quita**, para que entre los dos bornes de salida no
quede más que el fototransistor desnudo.

**Dos comprobaciones con multímetro antes de comprar cuatro canales**, con el
módulo sin alimentar:

1. **Con el jumper fuera**, medir entre los dos bornes de salida de un canal:
   debe dar abierto en los dos sentidos. Si marca una resistencia, el jumper no
   desconecta lo que creemos y el módulo no sirve tal cual.
2. Medir entre la `G` de la salida del canal 1 y la `G` del canal 2: **debe dar
   abierto**. Si hay continuidad, los dos canales comparten un nodo de salida y
   solo servirían si los botones del mando también lo comparten — cosa que no se
   sabrá hasta capturar el bus.

*El GND compartido del lado de **entrada** no es problema: las cuatro entradas
salen del ESP32, que ya comparte masa consigo mismo.*

**Alternativa más limpia:** comprar **chips PC817 sueltos** (DIP-4) y montarlos
en protoboard. Más baratos todavía y sin circuitería de módulo que neutralizar.

**Sin verificar, y bloqueante antes de decidir:** que la corriente por el botón
vaya siempre en el mismo sentido. Un PC817 conduce en una sola dirección; un
photoMOS y un botón, en las dos. Lo responde la captura del bus, no una
fotografía. **Hasta entonces esto es un candidato, no una decisión** — el ADR
vigente sigue siendo [ADR-017](DECISIONS.md).

---

## Consumibles y herramienta

| Artículo | Por qué | ~€ |
|---|---|---|
| Cable **AWG 30** de wire-wrap, o silicona **AWG 28** | Las tres derivaciones al conector del mando | 5 |
| Cinta Kapton | Alivio de tracción sobre las derivaciones — no es opcional, un tirón arranca la pista | 4 |
| Malla desoldadora y flux en lápiz | Para cuando una junta salga mal, que saldrá | 5 |
| Protoboard y jumpers M-M y M-H | Montar la sonda | 6 |
| Tercera mano con lupa | Soldar AWG 30 a paso de 2 mm con las dos manos ocupadas | 10 |
| Puntas finas para el multímetro | Medir patas a 1.27 mm sin puentear las vecinas. Ya hubo una medición dudosa por esto | 5 |

---

## No hace falta

- **Osciloscopio.** El analizador lógico cubre todo lo que este proyecto
  necesita ver, y cuesta veinte veces menos.
- **Conectores JST.** No se corta ningún cable: las derivaciones se sueldan
  dejando el conector original puesto.
- **Fuente de alimentación.** El ESP32 va por USB durante toda la ingeniería
  inversa ([ADR-007](DECISIONS.md)), y después con un cargador de móvil
  cualquiera.
- **Nada para las fases 4 y 5.** Son software.
