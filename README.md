# Escritorio elevable automatizado — ESP32 + Home Assistant

Automatizar un escritorio elevable **Jiecang** (vendido como Cougar) mediante
ingeniería inversa del bus entre su caja de control y el mando físico: leer la
altura real en todo momento y moverlo desde Home Assistant.

**Sin abrir la caja de control, sin modificar el mando y sin perder su uso
manual.** El mando físico sigue funcionando exactamente igual, y es el botón de
pánico del sistema.

> **Estado: funcionando.** El escritorio se lee y se controla desde Home
> Assistant con 19 entidades. El proyecto está documentado para poder retomarse
> leyendo solo `docs/`.

---

## Qué hace

- **Lee la altura** en tiempo real, decodificando el bus del mando
- **Mueve el escritorio a una altura concreta** — se pide 95 cm y va, frenando solo
- **Expone todo a Home Assistant** por MQTT: altura, estado, salud del enlace, botones
- **Detecta el uso manual**: sabe cuándo una persona ha tocado el mando, no solo cuándo lo movió el ESP32

## Cómo funciona

El mando lleva un **AiP650E** (clon del TM1650), un controlador de display y
teclado que habla con la caja de control por un bus de dos hilos tipo I²C
**sin direccionamiento**, a ~202 kHz.

**Leer** — una sonda resistiva deriva las dos líneas hacia el ESP32, que
muestrea a 4 MHz por ráfagas y decodifica las tramas. El bus nunca se escribe.

**Actuar** — cuatro **PC817** en paralelo con los pulsadores del mando, como
contactos secos aislados galvánicamente. El ESP32 "pulsa botones"; la caja de
control no distingue esos toques de un dedo.

```
caja de control ──bus 2 hilos──> mando (AiP650E)
                      │                  │
                   sonda            4 pulsadores
                      │                  │
                      └──> ESP32 <───PC817 x4
                             │
                          WiFi/MQTT ──> Home Assistant
```

## Lo medido

Todo verificado contra el hardware real, no deducido de datasheets:

| | |
|---|---|
| Códigos de tecla | subir `0x47`, bajar `0x57`, memorias `0x67` y `0x6F` |
| Rango físico | 73 – 118 cm |
| Velocidad | 0.68 cm/s, igual en ambos sentidos |
| Inercia tras frenar | ~1 cm |
| Mínimo para registrar tecla | 160 ms |
| Toque → movimiento continuo | 2.2 – 2.6 s |
| Toque → **grabar** preset | 3.0 s |
| Salud del bus en reposo | ~0.7% de tramas malformadas |

Esos dos últimos umbrales son los que gobiernan el diseño: **nada accesible
desde el móvil puede mantener un contacto tanto tiempo.**

## Seguridad

Un escritorio que se mueve solo puede hacer daño. Las protecciones, en capas:

- **Físicas** — topes mecánicos, y el mando siempre conectado como parada
- **Aislamiento galvánico** — ninguna tensión del ESP32 puede llegar a la caja
- **Tiempos acotados** — los anchos de pulso quedan lejos de los umbrales peligrosos
- **Watchdog** — un cuelgue con un contacto cerrado reinicia el chip y lo abre
- **Supervisión de viaje** — frena por objetivo, por límite, por lectura obsoleta, por estancamiento, por dirección invertida y por tiempo máximo; y **verifica que frenó**
- **Vigilancia externa** — Home Assistant avisa al móvil si algo se mueve más de 3 minutos, si el enlace se degrada o si el ESP32 desaparece

⚠️ **Límite conocido:** si el ESP32 muere en mitad de un viaje, nada lo frena por
software — frenar exige *cerrar* un contacto. Quedan el tope físico y el mando.

## Documentación

El proyecto se documenta con una disciplina explícita
([POLITICA_DOCUMENTACION.md](docs/POLITICA_DOCUMENTACION.md)): cada afirmación
técnica marcada como **verificada, supuesta o descartada**, decisiones
irreversibles en ADR, capturas crudas sin editar, y **nada de correcciones
silenciosas** — lo que estaba mal se corrige *y se registra que estaba mal*.

| Documento | Qué contiene |
|---|---|
| [PLAN.md](docs/PLAN.md) | Fases y siguiente paso concreto |
| [DECISIONS.md](docs/DECISIONS.md) | 33 ADR con su porqué |
| [BITACORA.md](docs/BITACORA.md) | Diario de sesiones, incluidos los errores |
| [PROTOCOLO.md](docs/PROTOCOLO.md) | El bus, descifrado |
| [HARDWARE.md](docs/HARDWARE.md) | Mediciones y montaje |
| [SEGURIDAD.md](docs/SEGURIDAD.md) | Riesgos y reglas |
| [INTEGRACION_HA.md](docs/INTEGRACION_HA.md) | Qué se expone a Home Assistant |
| [capturas/](docs/capturas/) | Volcados crudos del bus, con su contexto |

La bitácora incluye los caminos equivocados a propósito: un mando dado por
quemado que era un cortocircuito de estaño, un canal "muerto" que era un fallo
del propio sniffer, y dos rondas de revisión adversarial que encontraron
regresiones introducidas por los arreglos anteriores.

## Montarlo

**Hace falta:** un ESP32 DevKit, 4 optoacopladores PC817, resistencias, y un
escritorio Jiecang con mando `JK-CH506` o compatible.

1. `firmware/desk_sniffer/secrets.h.example` → `secrets.h`, con tus credenciales
2. Compilar y cargar con `arduino-cli` (ver [firmware/README.md](firmware/README.md))
3. Un broker MQTT alcanzable; las entidades aparecen solas por discovery

⚠️ **Antes de tocar hardware**, leer [SEGURIDAD.md](docs/SEGURIDAD.md). El
orden de conexión importa: **USB primero, hilos del bus después.**

## Licencia

MIT — ver [LICENSE](LICENSE).

El datasheet de `docs/hardware/datasheets/` es del fabricante y se incluye
porque el original ya no es descargable; sus derechos son de I-CORE.
