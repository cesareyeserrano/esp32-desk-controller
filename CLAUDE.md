# Instrucciones para agentes

Proyecto de ingeniería inversa de hardware: automatizar un escritorio elevable
Jiecang con un ESP32, leyendo el bus entre su caja de control y el mando físico.

## Antes de hacer nada

Lee [docs/POLITICA_DOCUMENTACION.md](docs/POLITICA_DOCUMENTACION.md). Rige sobre
todo lo demás y aplica igual a personas y a agentes.

Después, para saber dónde estamos: [docs/PLAN.md](docs/PLAN.md) y la entrada más
reciente de [docs/BITACORA.md](docs/BITACORA.md).

## Lo que más importa aquí

**Un error cuesta hardware, no un test rojo.** Un paso a la vez, verificando
antes de avanzar. Antes de proponer cualquier conexión física, comprobar el
cálculo eléctrico contra [docs/SEGURIDAD.md](docs/SEGURIDAD.md).

**No se reabre una decisión sin evidencia nueva.** Las decisiones están en
[docs/DECISIONS.md](docs/DECISIONS.md) con su porqué. Una opinión distinta no es
evidencia. Si hay evidencia nueva, se escribe un ADR que reemplaza al anterior;
el anterior no se toca.

**No hay osciloscopio ni analizador lógico.** El ESP32 tiene que hacer de
instrumento. Cualquier propuesta que dependa de instrumentación que no existe
está fuera de alcance.

**Separar medido de supuesto.** Al escribir en la documentación, marcar cada
afirmación técnica como verificada, supuesta o descartada. Un supuesto que se
escribe como hecho es cómo se conecta 5 V a un GPIO.

**Toda implementación pasa revisión adversarial antes de darse por terminada.**
Regla fijada por el propietario el 2026-08-23, tras dos rondas en las que la
revisión encontró: un limitador de seguridad prometido que nunca se implementó,
tres botones que no funcionaban, y regresiones introducidas por los propios
arreglos de la ronda anterior. Los hallazgos se verifican contra el código antes
de aceptarlos, y lo que se decide no arreglar se anota como aceptado, no se
omite.

**Nada de correcciones silenciosas.** Si algo documentado estaba mal, se corrige
y se registra que estaba mal.

## Al cerrar una sesión

Entrada de bitácora, documentos temáticos actualizados, siguiente paso concreto
en PLAN.md y README.md, y el ADR si hubo decisión. Criterio: si se pierde toda
la memoria de la conversación, ¿se puede retomar leyendo solo `docs/`?

## Idioma

Prosa en español. Nombres de archivo, código y comentarios del firmware, en
inglés.
