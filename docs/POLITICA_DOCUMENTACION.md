# Política de documentación

Este es un proyecto de ingeniería inversa de hardware, no de software normal.
Lo que se pierde entre sesiones no es el código: son las mediciones, los
supuestos y la razón por la que se descartó algo. La política existe para eso.

Rige sobre todo lo que hay en `docs/`. Aplica igual si escribe una persona o un
agente.

---

## Las cinco reglas

### 1. Cada sesión con hardware deja una entrada de bitácora

Sin excepción. Aunque no funcione nada. Aunque la sesión dure diez minutos.
Una sesión que no dejó entrada es una sesión que se va a repetir.

La entrada se escribe **durante** la sesión, no al final de memoria. Se usa la
plantilla de [plantillas/entrada-bitacora.md](plantillas/entrada-bitacora.md) y
se añade al principio de [BITACORA.md](BITACORA.md) — lo más reciente arriba.

Obligatorio en cada entrada:

- **Números con unidades.** "El voltaje estaba bien" no es un dato. "4.32 V
  medidos en la línea amarilla contra azul" sí lo es.
- **Cómo se midió.** El instrumento, la escala, contra qué referencia. Una
  medición sin método no se puede repetir ni cuestionar.
- **Qué se esperaba antes de medir.** Escribirlo *antes* del resultado. Es lo
  que convierte una sorpresa en información en vez de en ruido.
- **Qué quedó a medias.** El estado físico del montaje al terminar: qué está
  conectado, qué está suelto, qué está soldado.

### 2. Las decisiones que cuesta revertir van a DECISIONS.md como ADR

Un ADR se escribe cuando la decisión afecta el montaje físico, define un camino
técnico, o descarta una alternativa. No para cada elección menor.

**Los ADR son inmutables.** Si una decisión cambia, se escribe uno nuevo que
declara `Reemplaza a ADR-NNN`, y el viejo pasa a `Estado: Reemplazado por
ADR-MMM`. Nunca se edita el texto de un ADR publicado, ni se borra. El valor
está justo en poder ver qué se creía antes y por qué estaba mal.

Todo ADR lleva: contexto, decisión, y **qué se pierde** al tomarla. Un ADR sin
costo declarado está incompleto — significa que no se pensó la alternativa.

### 3. Las capturas crudas se guardan sin editar

Todo volcado del sniffer va a [capturas/](capturas/) tal cual salió, con el
nombre `AAAA-MM-DD-descripcion.log`. No se recortan, no se limpian, no se
reordenan. La interpretación va aparte, en [PROTOCOLO.md](PROTOCOLO.md),
citando el archivo y la línea.

Cada captura necesita una cabecera de contexto — qué estaba haciendo el
escritorio mientras se grababa. Una captura sin contexto es un archivo de
números inservible. Formato en [capturas/README.md](capturas/README.md).

### 4. Las fuentes primarias se guardan dentro del proyecto

Todo datasheet o documento del que dependa una decisión se copia a
[hardware/datasheets/](hardware/datasheets/). Un enlace no basta.

La razón no es teórica: el datasheet del AiP650E, del que dependen cuatro ADRs,
está tras una protección que devuelve 403 a cualquier descarga automática. Y
antes de tenerlo, una tabla de pinout equivocada de un agregador estuvo a punto
de decidir dónde soldar. Los enlaces caducan, cambian y mienten; el PDF que
está en el repositorio, no.

En [REFERENCIAS.md](REFERENCIAS.md) va el enlace **y qué se sacó de él**. Un
enlace sin esa frase no dice, dentro de seis meses, por qué estaba ahí.

### 5. Se separa lo medido de lo supuesto

En toda la documentación, cada afirmación técnica se marca:

- **Verificado** — se midió o se observó directamente. Se dice cómo.
- **Supuesto** — es razonable pero no se comprobó. Se dice qué comprobación
  lo confirmaría.
- **Descartado** — se probó y falló. Se dice cómo falló.

Esta es la regla que más se rompe y la que más caro sale. El handover de origen
decía "ROJO: VCC (5 V) probable" — esa palabra *probable*, mantenida, es lo que
evita conectar 5 V a un GPIO por confiar en una etiqueta.

---

## Qué archivo toca

Antes de escribir, ubica en cuál va:

- ¿Pasó en una sesión? → **BITACORA.md**
- ¿Es una elección con alternativas descartadas? → **DECISIONS.md**
- ¿Es un hecho físico del hardware? → **HARDWARE.md**
- ¿Es algo que se descifró del bus? → **PROTOCOLO.md**
- ¿Cambia el orden o el alcance del trabajo? → **PLAN.md**
- ¿Puede lastimar a alguien o quemar algo? → **SEGURIDAD.md**

Un hecho vive en **un** archivo. Los demás enlazan. Duplicar es garantizar que
en tres meses las dos copias digan cosas distintas y no se sepa cuál vale.

---

## Cómo se cierra una sesión

Cuatro cosas, en orden:

1. Entrada de bitácora completa.
2. Los archivos temáticos actualizados con lo aprendido (HARDWARE, PROTOCOLO).
3. El **Siguiente paso** del [PLAN.md](PLAN.md) y del [README](../README.md)
   apuntando a la acción concreta que sigue — no a un área vaga.
4. Si hubo una decisión, su ADR escrito.

El criterio: si mañana se pierde toda la memoria de la conversación, ¿alguien
puede retomar leyendo solo `docs/`? Si no, la sesión no está cerrada.

---

## Sobre el trabajo asistido por agentes

Las sesiones anteriores cambiaron de recomendación varias veces (relés → cable
→ relés → cable) sin dejar registro de por qué. Eso es lo que esta política
previene. Reglas específicas:

- **No se reabre una decisión sin evidencia nueva.** Si se reabre, la evidencia
  se cita en el ADR que la reemplaza. Una opinión distinta no es evidencia.
- **Un paso a la vez, verificando antes de avanzar.** El costo de un error aquí
  es hardware quemado, no un test rojo.
- **Nada de correcciones silenciosas.** Si algo documentado estaba mal, se
  corrige *y* se registra que estaba mal. Un documento que se arregla solo,
  sin rastro, es un documento en el que no se puede confiar.

---

## Idioma y formato

**Cambiado el 2026-09-02.** Antes: toda la prosa en español. Ahora el
repositorio es público y el idioma depende de para quién es cada documento:

| Documento | Idioma | Por qué |
|---|---|---|
| README, PLAN, PROTOCOLO, HARDWARE, SEGURIDAD, INTEGRACION_HA, PCB, README de capturas y firmware | **Inglés** | Es lo que lee quien llega al repositorio desde fuera |
| BITACORA y DECISIONS | **Español** | Ver abajo |
| Nombres de archivo, identificadores y comentarios del firmware | **Inglés** | Sin cambio |

**La bitácora y los ADR se quedan en español a propósito, y no es pereza.** Son
un diario escrito *mientras* pasaban las cosas, con la distinción entre medido,
supuesto y descartado incrustada en el modo de decirlo. Traducirlos a posteriori
significa reescribir cientos de afirmaciones marcadas, y **la forma más fácil de
estropear un supuesto es traducirlo con prisa hasta que suene a hecho** — que es
exactamente contra lo que existe la regla 5. Los ADR además son inmutables: una
traducción es una edición.

Los nombres de archivo se mantienen como estaban (`SEGURIDAD.md`,
`PROTOCOLO.md`, `INTEGRACION_HA.md`) aunque el contenido esté en inglés.
Renombrarlos rompería los enlaces desde la bitácora y los ADR, que **no se
tocan**.

Formato: Markdown plano, líneas de hasta ~80 columnas, sin dependencias ni
herramientas de generación.
