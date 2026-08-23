# Seguridad

> Se revisa antes de abrir cada fase nueva.

---

## Riesgo eléctrico (fases 2 y 3)

**El escritorio se desenchufa de la corriente para cualquier medición de
resistencia o continuidad.** Medir resistencia en un circuito alimentado da
lecturas falsas y puede dañar el multímetro.

**Nunca se fuerza SDA a alto con una salida push-pull.** Mientras el AiP650 tira
de la línea abajo, eso es un cortocircuito contra la salida del chip. El chip es
irreemplazable y el mando entero con él. Ver [ADR-011](DECISIONS.md).

**El cable de 6 hilos entre las dos columnas no se toca nunca.** Lleva los
**29 V** del adaptador y la corriente de los motores. No hay nada ahí que el
proyecto necesite.

**El ESP32 no se alimenta del escritorio, ni siquiera de sus 5 V.** Esos 5 V los
genera la caja de control para un chip que consume 0.3 mA; un ESP32 con WiFi
pega picos de cientos de miliamperios y no se sabe qué aguanta ese regulador.
USB desde el Mac, siempre. Ver [ADR-007](DECISIONS.md).

**El hilo amarillo (5 V) no se conecta al ESP32.** Nunca, en ninguna fase. El ESP32
se alimenta por USB ([ADR-007](DECISIONS.md)).

**Ninguna línea de 5 V toca un GPIO directamente.** Los GPIO del ESP32 toleran
3.6 V como máximo absoluto. Aplica a las líneas de datos ([ADR-005](DECISIONS.md))
y a los pulsadores ([ADR-004](DECISIONS.md)).

**Antes de conectar el ESP32, verificar que el mando funciona con las
derivaciones soldadas.** Si no se comprueba en ese orden, un fallo posterior es
imposible de atribuir: ¿la soldadura o la sonda?

**El ESP32 se alimenta por USB antes de conectar los hilos al divisor, y se
desconectan los hilos antes de quitar el USB.** Con el ESP32 sin corriente y la
sonda puesta sobre un bus encendido, el diodo de protección del GPIO engancha el
nodo del divisor y arrastra el bus hasta ~2.85 V, por debajo del umbral del
chip: el mando falla. No hay daño —la corriente son ~210 µA, limitados por los
9.1 kΩ— pero el síntoma aparecería justo después de soldar y apuntaría a las
soldaduras. Ver [ADR-019](DECISIONS.md).

**Se comprueba el nivel del bus con el multímetro antes de dar por bueno el
montaje, y el criterio es el cociente entre dos lecturas, no un umbral
absoluto.** Un multímetro promedia y el bus está conmutando, así que las dos
lecturas salen por debajo de lo calculado en una cantidad desconocida. Se mide
rojo↔azul y verde↔azul sin la sonda y con ella, con el escritorio quieto y el
mismo número en pantalla: el cociente debe rondar **0.80**, y por debajo de
**0.70** hay que desconectar. Procedimiento completo en
[HARDWARE.md](HARDWARE.md), razonamiento en [ADR-018](DECISIONS.md).

---

## Riesgo mecánico (fases 3 en adelante)

Un escritorio que se mueve solo puede atrapar manos, cables o mascotas. La
superficie sube con fuerza suficiente para levantar un monitor, y por tanto
suficiente para hacer daño.

Reglas para cuando exista accionamiento:

- **Límites de altura por software**, mínimo y máximo, verificados contra la
  altura leída del bus. Un comando fuera de rango no se ejecuta.
- **Ningún movimiento automático sin presencia.** Las automatizaciones que
  mueven el escritorio requieren confirmación de que hay alguien delante.
- **Parada disponible siempre.** El mando físico se mantiene conectado y
  funcional en todas las fases; es el botón de pánico.
- **Los pulsos de M1/M2 se acotan por hardware** ([ADR-010](DECISIONS.md)). Un
  cierre largo graba el preset en vez de recuperarlo, y falla en silencio.
- **Un ESP32 colgado no es hipotético: se colgó dos veces el 2026-08-03.** No
  por un bug de lógica, sino por una condición eléctrica en un pin — una señal
  a 175 kHz lo dejó sin responder, y no degradó suavemente.

  **Corrección del 2026-08-06, porque esta evidencia se citaba con más fuerza de
  la que tiene:** aquellos cuelgues fueron por **saturación de interrupciones**,
  con `attachInterrupt` enganchado a los dos pines. **Ese diseño de captura ya no
  existe**: el sniffer pasó a muestreo por ráfagas y no engancha interrupciones a
  esas líneas. El mecanismo concreto que se demostró está eliminado.

  Lo que queda en pie es lo genérico —cualquier firmware puede colgarse por otras
  vías, y el actual desactiva interrupciones 2 ms cada ráfaga— pero **esto es una
  precaución razonable, no un requisito con evidencia directa.** La diferencia
  importa cuando decidir implica comprar componentes.

  Lo que no cambia: **lo único que puede parar un canal cerrado con el firmware
  muerto es hardware que no dependa del CPU.**
- ⚠️ **Los cables del pulsador van a las patas 3 y 4 del PC817, nunca a la 1 y
  2.** Las 1 y 2 son el LED y ya llevan el ESP32; las 3 y 4 son el interruptor
  aislado. Soldar el pulsador a las patas 1 y 2 uniría los 5 V del mando con el
  GPIO del ESP32 y anularía el aislamiento entero.

- **Los cuatro canales llevan el mismo limitador de pulso de 300 ms**
  ([ADR-023](DECISIONS.md)). Decidido el 2026-08-06, cerrando lo que bloqueaba
  la fase 3. **El firmware solo emite toques; el hardware garantiza que ninguno
  dure más.**

  **Corrección del 2026-08-23, y es incómoda:** desde que se escribió esto
  hasta hoy, **ese watchdog no existía**. [ADR-024](DECISIONS.md) lo decidió y
  nadie lo implementó: el firmware no configuraba ningún watchdog, y el ancho
  del pulso lo acotaba el mismo software que podría colgarse. La promesa de
  este documento era papel. Lo destapó la revisión adversarial del 2026-08-23
  ([REVISION_FIRMWARE_2026-08-23.md](REVISION_FIRMWARE_2026-08-23.md), hallazgo
  B1).

  **Desde el 2026-08-23 sí existe:** cada pulso arma el task watchdog del ESP32
  con presupuesto de `ancho + 1 s` y pánico al vencer. Un cuelgue con el pin en
  alto termina en reinicio del chip, el reinicio deja los GPIO como entradas, y
  el canal se abre. Se desarma al soltar el pin. Verificado en operación normal:
  no salta con pulsos legítimos.

  **Y el propio pulso ahora se mide:** al soltar se compara el ancho conseguido
  contra el pedido, y un desborde de más de 100 ms se reporta con `[!!]`. Antes
  la decodificación del bus se hacía **dentro** del contacto y podía alargarlo
  sin que nadie lo supiera (hallazgo B2); ahora se captura durante y se
  decodifica después de soltar.

  ⚠️ **Antes de cablear nada hay que medir el nivel de los GPIO de control
  durante el arranque y el reset.** Toda esta protección se apoya en que un
  reinicio deje los pines sin conducir. **Si alguno se pone alto al arrancar, el
  watchdog activaría el canal en vez de abrirlo.**

  Los umbrales medidos que lo justifican: un toque pasa a **movimiento continuo**
  entre **2.2 y 2.6 s**, y M1/M2 pasan a **grabar el preset** a los **3.0 s**.
  Con 300 ms hay siete veces de margen por abajo, y casi el doble del mínimo de
  160 ms que el chip exige para ver la pulsación.

  Así **un contacto pegado no puede arrancar movimiento continuo ni sobrescribir
  un preset**: como mucho mueve un centímetro o recupera una memoria.

- ⚠️ **El movimiento continuo no se puede parar abriendo un contacto.** Medido:
  tras soltar, el escritorio siguió **5 cm en 5 s** subiendo y **6 cm en 6.6 s**
  bajando. Para detenerlo hay que **cerrar** un contacto, no abrirlo — por eso
  ningún circuito que actúe por ausencia de señal sirve aquí.

  **Cambiado el 2026-08-21 ([ADR-028](DECISIONS.md)).** El diseño era *evitar que
  el movimiento continuo llegue a arrancar*. Ahora **se admite**, porque se midió
  lo que faltaba: **un toque corto en el MISMO canal frena su propio movimiento
  continuo**. Con un solo canal cableado hay parada.

  ⚠️ **Riesgo nuevo, y es de otra clase:** un cuelgue del ESP32 durante el viaje
  deja el escritorio moviéndose **sin nada que lo pare**. El watchdog de
  [ADR-024](DECISIONS.md) **no ayuda** — abre el canal, y abrir no frena. Queda
  el tope físico y el mando. **Es una mitigación, no una garantía.**

  **Por eso: movimiento continuo solo con supervisión**, mientras no existan los
  límites por software con la altura leída del bus. Los toques
  ([ADR-027](DECISIONS.md), 800 ms) no tienen este riesgo y siguen siendo el modo
  por defecto.
- **Freno disponible siempre.** Cualquier botón detiene un movimiento en curso,
  así que cualquier relé sirve de parada de emergencia. Ante una lectura de
  altura incoherente, frenar es la acción por defecto, no seguir.
- **Con altura obsoleta no se inicia movimiento** ([ADR-012](DECISIONS.md)). Si
  el refresco del display se detiene o salta de forma incoherente durante un
  movimiento, se frena.
- **Sin bucles de reintento.** Si un movimiento no llega a la altura esperada,
  se detiene y se reporta. No se reintenta solo: un sensor mal decodificado
  reintentando indefinidamente es un escritorio golpeando contra un tope.
- **El botón de reset no se cablea** ([ADR-008](DECISIONS.md)). Ejecuta una
  recalibración que baja la superficie hasta el tope inferior, sin confirmación
  y sin poder interrumpirse. Se deja físicamente fuera del circuito: no es una
  comprobación de software que un bug pueda saltarse.

---

## Riesgo de perder el escritorio

La caja de control es propietaria y no se abre. Si se corrompe su configuración
o se daña el mando, no hay repuesto inmediato ni forma de diagnosticarlo.

De ahí la fase de solo lectura ([ADR-002](DECISIONS.md)): mientras no se escriba
en el bus, ningún error del ESP32 puede alterar el estado del escritorio.
