# Revisión adversarial del firmware — 2026-08-23

> Pedida antes de exponer más controles a Home Assistant, sobre un checkpoint
> estable. Cinco pasadas independientes (una humana + cuatro agentes con ángulos
> distintos), consolidadas y **cada hallazgo crítico verificado contra el
> código** antes de entrar aquí. Referencias a `firmware/desk_sniffer/desk_sniffer.ino`.

**Veredicto: NO está listo para exponerse sin supervisión.** Hay botones de HA
que no funcionan en absoluto, y varias promesas de seguridad escritas en la
documentación que el código no cumple.

---

## A. Roto — funcionalidad que directamente no funciona

### A1. Tres botones de HA son no-ops silenciosos ⚠️ VERIFICADO

La guardia de longitud del callback MQTT (línea ~1035) rechaza `len >= 9`.
Consecuencia: **descarta sin log** los payloads `refrescar` (9), `continuo_subir`
(14) y `continuo_bajar` (14) — tres de los botones que el propio discovery
anuncia. **"Subir continuo" y "Bajar continuo" jamás han funcionado desde HA**;
además `g_pendingCmd[12]` ni siquiera podría contenerlos.

*(El `refrescar` por MQTT que se probó con éxito el 2026-08-22 fue **antes** de
añadir esa guardia — la cronología cuadra.)*

### A2. Transitorios del display se aceptan como alturas reales ⚠️ VERIFICADO

El filtro de altura (línea ~529) solo comprueba el rango 73–118. Pero el display
se refresca dígito a dígito: yendo de 089 a 090 muestra **"099"** durante un
ciclo — y 99 está dentro del rango, así que **se acepta como altura real**. Las
capturas de los recorridos lo muestran ("099", "199", "089"...).

Consecuencias: freno por "objetivo" a 6 cm del objetivo real, resets falsos del
detector de estancamiento, y posibles frenos de límite falsos. **Los guiones
Python tenían filtro de salto (±3 cm); el firmware no lo heredó.**

### A3. Un viaje de 1–2 cm ejecuta un pulso largo entero ⚠️ VERIFICADO

`startTravel` siempre usa `pulseChannelLong` (línea ~1129). Pedir 96 desde 95
arranca 2.8 s de movimiento continuo, sin supervisión durante el pulso, y llega
a ~99 antes de frenar; luego el ajuste fino caza de vuelta. **El guion Python
usaba toques para huecos ≤2 cm; el firmware no.**

---

## B. Seguridad — promesas escritas que el código no cumple

### B1. El limitador de pulso "garantizado por hardware" no existe ⚠️ VERIFICADO

[SEGURIDAD.md](SEGURIDAD.md) promete *"el hardware garantiza que ningún pulso
dure más"* apoyándose en el watchdog de [ADR-024](DECISIONS.md). **No hay ningún
watchdog configurado en el firmware** (cero `esp_task_wdt`). El ancho lo acota el
mismo software que podría colgarse con el pin en alto. Un cuelgue dentro del
pulso: >2.2 s arranca continuo, >3.0 s **sobrescribe un preset**.

### B2. TAIL_MS no acota nada: el pulso largo puede pasarse de 3.0 s ⚠️ VERIFICADO

Dentro del pulso se captura y decodifica con presupuesto de 20 ms (línea ~198),
pero `decodeBurst` imprime por serie **sin límite** (`g_raw` es `true` por
defecto, línea 391). Con el buffer TX lleno, `Serial.print` bloquea a 115200 —
vaciar 4 KB son ~356 ms. Una ronda que arranca a los 2779 ms de un pulso de
2800 puede soltar el contacto **pasados los 3.0 s que graban preset**. El margen
es 200 ms y una sola ronda bloqueada lo consume.

### B3. Los límites miran la dirección ORDENADA, no la observada ⚠️ VERIFICADO

`superviseTravel` (líneas 1144-1145) solo comprueba el techo si cree que sube y
el suelo si cree que baja. **Si el escritorio se mueve al revés de lo ordenado**
—un canal mal cableado, el fallo exacto que este proyecto ya tuvo—, ningún
límite ni objetivo puede dispararse jamás: la altura cambia (resetea el
estancamiento) y atraviesa el suelo de software hasta el tope físico.

### B4. `A B C D` por serie: viaje continuo sin NINGUNA supervisión ⚠️ VERIFICADO

`pulseChannelLong` directo, sin tocar `g_motion`: límites, altura obsoleta,
estancamiento y tope de 90 s **no aplican**. Y `C`/`D` meten **2.8 s de contacto
en los canales de memoria** — a 200 ms de grabar el preset (ver B2).

### B5. El freno no se verifica

`stopTravel` pone `MOTION_IDLE` **antes** de emitir el toque de freno y nunca
comprueba que el escritorio paró. Si el toque no registra (la clase de fallo del
2026-08-21), la supervisión ya está desarmada: sin reintento, sin aviso, con el
escritorio andando y MQTT publicando "quieto". Falta un estado `BRAKING` que
vigile hasta confirmar la parada.

### B6. El mensaje retenido de comando no se borra del broker

La defensa es una ventana de 4 s de reloj — pero una pasada del loop puede
bloquear 5.6 s (wake + espera de frescura + pulso largo), y el retenido llega
**después** de expirar la ventana: se ejecuta. Reproduce el incidente del
2026-08-22 en cada reconexión con el loop ocupado. Arreglo real: **publicar un
retenido vacío al conectar** para borrar el del broker, no adivinar por tiempos.

---

## C. Comportamiento incorrecto — menor que B, real igualmente

- **C1. `parar` en reposo mueve el escritorio**: toque de **800 ms en subir**
  (línea 1201). Pulsaciones repetidas de pánico lo suben cm a cm. El comentario
  "harmless tap" es falso desde ADR-027. VERIFICADO.
- **C2. `refrescar` durante un viaje lo frena físicamente** (el wake cierra un
  contacto) pero el estado sigue en "viajando": 9 s después salta un freno
  "no avanza" falso que además mueve 1 cm. Las dos reglas —"el wake no mueve" y
  "un toque en viaje es parar"— se contradicen en este camino.
- **C3. `ir:N` durante un viaje se traga el objetivo**: actúa como freno y
  descarta la altura pedida sin error; el `number` de HA luego se sobreescribe
  con la altura real y parece aceptado.
- **C4. Cola de comandos de profundidad 1**, sobrescritura sin log. `parar` es
  el comando que nunca debería perderse; merece un flag propio fuera de la cola.
- **C5. El ajuste fino no se cancela desde serie** y no comprueba límites; puede
  disparar toques mientras alguien opera por consola.
- **C6. `altura` se publica retenida sin regla de frescura**: HA puede consumir
  un número de hace horas presentado como actual (fuga de ADR-012). Publicar
  `unknown` al superar STALE_MS.
- **C7.** La ventana de asentamiento del ajuste (2500 ms) se calcula **antes**
  del pulso de freno de 800 ms: son realmente 1700 ms.

## Lo que salió limpio

Nunca dos canales a la vez (verificado), pines del bus solo-lectura para siempre
(verificado), inyección MQTT (`ir:basura` → rechazado), carrera del callback
(mismo hilo), rango 73–118 en `startTravel`, y toda rama de `superviseTravel`
termina en freno — aunque B3 hace dos ramas inalcanzables y B4 la esquiva entera.

## Estado de los arreglos

| Hallazgo | Estado |
|---|---|
| **B1** — watchdog inexistente | ✅ **Arreglado 2026-08-23**: task WDT armado en cada pulso (`ancho+1 s`, pánico→reset→GPIO a entrada). Verificado: no salta en operación normal |
| **B2** — el pulso podía alargarse imprimiendo | ✅ **Arreglado 2026-08-23**: se captura durante el contacto y se decodifica **después** de soltar; el ancho conseguido se mide y un desborde >100 ms se reporta con `[!!]`. Verificado: 800 ms exactos y la atribución de tecla sigue funcionando |
| **A1** — 3 botones de HA rotos | ✅ **Arreglado**: buffer a 24 bytes, guardia por topic, y rechazo **con log**. Verificado: `continuo_subir` arrancó un viaje desde HA por primera vez |
| **A2** — transitorios del display como alturas | ✅ **Arreglado**: un salto >3 cm debe repetirse en el ciclo siguiente para creerse. Verificado en vivo: "199" y "109" en la transición 99→100 no perturbaron el viaje |
| **B3** — límites solo en la dirección ordenada | ✅ **Arreglado**: límites sobre la altura observada en ambas direcciones (activos solo al acercarse, para poder salir de un tope) + freno por "dirección invertida" si el escritorio va al revés de lo ordenado |
| **C1** — `parar` en reposo movía el escritorio | ✅ **Arreglado**: freno de **300 ms** siempre — registra como tecla (frena viajes de la caja: M1/M2/mando) y no mueve. El primer intento fue un no-op y era peor: habría dejado sin freno remoto los viajes que no inicia el ESP32 |
| **C2** — `refrescar` en viaje frenaba sin registrarlo | ✅ **Arreglado**: ya no está exento; en viaje, cualquier comando frena y lo dice |
| **C3** — `ir:N` en viaje se tragaba el objetivo | ✅ **Arreglado**: reobjetiva — frena con motivo "reobjetivo" y ejecuta el nuevo destino |
| **C7** — ventana de asentamiento mal medida | ✅ **Arreglado**: se calcula tras abrir el contacto |
| Freno de la supervisión movía 1 cm | ✅ **Arreglado**: todos los frenos usan 300 ms |
| **A3** — viaje corto usa pulso largo | ✅ **Arreglado**: huecos ≤2 cm van directos a toques. Verificado: 80→82→80 exactos sin pulso largo |
| **B5** — freno sin verificar | ✅ **Arreglado**: estado `MOTION_BRAKING` — vigila hasta que la altura asienta 1.5 s, un reintento a los 4 s, y si falla dos veces lo dice con `[!!] FRENO FALLIDO` en vez de callar. Verificado en dos viajes largos |
| **B6** — retenidos no se borran del broker | ✅ **Arreglado**: al conectar se publica un retenido vacío en los dos topics de comando ANTES de suscribirse — la clase de mensaje peligrosa deja de existir. La ventana de 4 s queda de respaldo |
| **C4** — `parar` podía perderse en la cola | ✅ **Arreglado**: `parar` viaja en un flag propio (`g_stopReq`) que nada sobrescribe, se atiende antes que todo, y salta la ventana de armado — un stop repetido es inofensivo, uno perdido no |
| **C5** — ajuste fino no cancelable desde serie | ✅ **Arreglado**: todo comando de canal por serie lo cancela |
| **C6** — `altura` retenida sin frescura | ✅ **Arreglado**: con la lectura obsoleta se publica `unknown`; la edad queda para diagnóstico. El `number` ya no hace eco de alturas viejas |
| **B4** — `A-D` por serie sin supervisión | ✅ **Arreglado**: `A`/`B` pasan por `startTravel` (límites, frenos). **`C`/`D` eliminados**: 2.8 s sostenidos en un canal de memoria quedan a 200 ms de GRABAR el preset, y para identificar basta el toque |

**16 de 16 cerrados el 2026-08-23.** Copia del firmware previo en
`firmware/backups/desk_sniffer_2026-08-23_9arreglos.ino.bak`.

## Orden de arreglo propuesto

1. **A1** (botones rotos) + **C1** (`parar` que mueve) — trivial y visible
2. **A2** (filtro de salto de altura) — contamina todo lo demás
3. **B2 + B1** (acotar el pulso de verdad: decodificar tras soltar, y watchdog real)
4. **B3** (límites en ambas direcciones, siempre)
5. **B5 + C4** (estado BRAKING + flag de parada imborrable)
6. **B6** (borrar retenidos al conectar)
7. **A3, C2, C3, C5, C6, C7**


---

# Ronda 2 — revisión de todo el sistema antes de publicar (misma fecha)

Ocho ángulos nuevos sobre el firmware **ya arreglado** y las herramientas.
Resultado que justifica la ronda: **el estado BRAKING de la ronda 1 introdujo
regresiones que tres ángulos encontraron por separado**, y las herramientas
Python quedaron desfasadas del firmware que evolucionó debajo.

## Arreglado y compilado (pendiente de flasheo: el ESP32 corre en su cargador)

**Regresiones del BRAKING:**
- **Reobjetivo roto otra vez**: `ir:N` durante un viaje se descartaba porque el
  freno deja BRAKING y el viaje nuevo se rechazaba. Ahora el objetivo **espera**
  a que el freno confirme y se ejecuta solo
- **El ajuste fino se cancelaba** si el frenado tardaba más de 2.5 s (carrera
  entre temporizadores; en las pruebas en vivo ganó por casualidad). Ahora
  espera durante BRAKING en vez de cancelarse
- **Reentrada de `parar` durante el frenado** reseteaba la escalada del freno
  fallido justo cuando más importaba. Ahora da un toque extra sin resetear
- **Freno con altura obsoleta**: abandonaba a ciegas sin verificar. Ahora
  intenta despertar el display y, si sigue ciego, lo dice con `[!!]` en vez de
  callar
- **HA veía "quieto" durante el frenado** (incluso con un freno fallido dos
  veces). Ahora publica **"frenando"**

**Seguridad de cola y radio:**
- **Un `subir` encolado podía ejecutarse DESPUÉS de un `parar`** llegado en el
  mismo lote. El stop ahora vacía la cola
- **La radio ya no se pone delante del freno**: durante un viaje no se reconecta
  ni se publica (la reconexión podía bloquear ~15 s = ~10 cm sin supervisar);
  solo se atiende la recepción, que es lo que trae el `parar`

**Unificaciones:**
- **Todos los toques inofensivos van al canal medido** (bajar, 300 ms — "13
  toques, cero deriva" + "el mismo botón frena su propio viaje", verificados).
  Había tres variantes y dos usaban un canal sin evidencia
- **Los dígitos por serie son SIEMPRE toques de 300 ms**: identifican y frenan
  sin mover. El movimiento queda solo en `A`/`B`/`ir:` supervisados
- 4 comentarios en español → inglés (regla del CLAUDE.md); casos muertos
  `C`/`D` eliminados; el viaje corto limpia el motivo de freno anterior

## Herramientas

- **`ir_a_altura.py` y `recorrido_prueba.py` → `tools/legacy/`**: competían con
  la máquina de estados del firmware (frenos cruzados, "tope alcanzado" falso).
  Los viajes son del firmware desde la fase 4
- **`pulse_loop.py`**: la cadencia (0.5 s) era más corta que el pulso (0.8 s) —
  los cierres se pegaban acercándose a una tecla MANTENIDA. Cadencia a 1.5 s
- **`verificar_canales.py`**: corta la escucha al identificar y frena las
  memorias al instante (antes las dejaba viajar ~5 s); su precondición de
  PULSE_MS ya no existe
- **`serial_talk.py`**: si el banner de arranque no llega, envía avisando en vez
  de salir con éxito sin haber enviado nada (el mismo patrón del falso "canal
  muerto" del 2026-08-21, una capa más arriba)

## Auditoría de publicación

- **Sin secretos en el repo**: ni contraseñas ni SSID; `secrets.h` ignorado con
  plantilla `.example`
- Dos IPs de LAN (192.168.x) en dos capturas: no rutables, riesgo nulo-bajo
- `.gitignore` completo

## Lo aceptado sin arreglar (deliberado)

- Duplicación de los bloques de discovery en el firmware y del bucle de lectura
  serie en las herramientas: deuda de limpieza, no de corrección
- `topic()` con String (fragmentación de heap a meses vista): anotado
- Publicaciones retenidas redundantes cada 5 s: coste ínfimo, anotado
- Reconexión MQTT bloqueante fuera de viajes: aceptable, el freno no depende de ella
