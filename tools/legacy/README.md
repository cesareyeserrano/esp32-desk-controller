# Herramientas retiradas — 2026-08-23

Estos guiones implementaban el control de viajes **en Python, sobre el puerto
serie**, cuando el firmware aún no sabía viajar. El firmware los superó: desde
la fase 4 los viajes viven dentro del ESP32 (`startTravel`/`superviseTravel`,
con límites, freno verificado y reobjetivo) y se ordenan por MQTT (`ir:N`,
`continuo_subir`, `parar`).

**No usarlos contra el firmware actual**: la revisión adversarial del
2026-08-23 (ronda 2) encontró que ahora **compiten con la máquina de estados
del firmware** — cada uno frena por su cuenta, el firmware interpreta esos
toques como movimiento, y ambos acaban metiendo frenos cruzados. Además su
freno era de 800 ms (mueve ~1 cm) y podían fabricar un "tope alcanzado" falso.

Se conservan como referencia histórica: su lógica de lazo cerrado es la que se
validó el 2026-08-22 y la que luego se portó al firmware.

Para mover el escritorio hoy:
    mosquitto_pub -t escritorio_jiecang/altura_objetivo/set -m 95
o los botones de Home Assistant.
