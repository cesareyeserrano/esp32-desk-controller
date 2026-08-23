# Referencias

Fuentes externas consultadas. Se anota **qué se sacó de cada una**, no solo el
enlace — un enlace suelto dentro de seis meses no dice por qué estaba ahí.

---

## Chip AiP650EO / TM1650

### Fuente primaria — I-CORE (el fabricante del chip real)

- **[Datasheet AiP650EO, Wuxi I-CORE (PDF)](https://www.mikrocontroller.net/attachment/626735/2403051032_Wuxi-I-core-Elec-Aip650EO_C5139014.pdf)**
  — *AiP650E Product Specification*, doc `AiP650E-AX-XS-B037EN`, ver.
  2024-01-B1, 15 páginas. **Es la fuente de verdad del proyecto**, y está
  leída entera. **Copia local archivada** en
  [hardware/datasheets/icore-aip650e-2024-01-b1.pdf](hardware/datasheets/icore-aip650e-2024-01-b1.pdf)
  — el enlace de arriba devuelve 403 a cualquier descarga automática, así que la
  copia del repositorio es la que vale.

  De aquí sale, verificado: el pinout completo, el nombre real de las líneas
  (**CLK** y **DIO**), el rango 3–5.5 V, el display de cátodo común, los
  **pull-ups internos de 550 µA ≈ 9.1 kΩ** que desbloquearon el diseño de la
  sonda ([ADR-013](DECISIONS.md)), los bytes de comando, el mapa de segmentos,
  **la tabla de códigos de teclado que confirma [ADR-011](DECISIONS.md)**, el
  modo sueño que sostiene [ADR-012](DECISIONS.md), y los tiempos de escaneo que
  fijan la duración mínima de un pulso de relé.

  Páginas útiles: 5 pinout, 6–7 parámetros eléctricos y tiempos, 7 direcciones
  de display y mapa de segmentos, 8 instrucciones y matriz de teclado, 9 tabla
  de códigos de tecla, 10 reglas de pulsación y puerto de comunicación,
  11 circuito de aplicación recomendado.
- [Ficha AiP650E en datasheet4u](https://datasheet4u.com/datasheets/I-CORE/AiP650E/1542664),
  [AiP650 en LCSC](https://www.lcsc.com/product-detail/LED-Drivers_AiP650_C132308.html),
  [AiP650EO en JLCPCB](https://jlcpcb.com/partdetail/Wuxi_I_coreElec-Aip650EO/C5139014),
  [AiP650 en semiee](https://www.semiee.com/90c4fce5-2f82-442f-80b8-114dd031f958.html)
  — fichas de catálogo. Confirman la descripción: 2 hilos, cátodo común,
  8 segmentos × 4 dígitos, escaneo de teclado 7×4 con soporte de algunas
  combinaciones.

**Resuelto:** el datasheet confirmó uno por uno los comandos, el mapa de
segmentos y el formato del byte de teclado que se habían tomado prestados de
fuentes del TM1650. Coinciden todos. La advertencia que había aquí ya no aplica.

**El TM1637 NO es referencia válida.** Aparece como "alternativa" en algunas
fichas, pero usa comandos distintos. Su documentación no aplica.

### Fuentes del TM1650 (compatible a nivel de software)

- [Datasheet TM1650 (Titan Micro, PDF)](https://components101.com/sites/default/files/component_datasheet/TM1650-Datasheet.pdf)
  y [versión V1.10 en mikrocontroller.net](https://www.mikrocontroller.net/attachment/568815/TM1650_V1.10-1.pdf)
  — datasheet original del chip que el AiP650EO clona.
- [components101 — TM1650 pinout y specs](https://components101.com/ics/tm1650-led-driver-ic)
  — **su tabla de pinout está MAL y se usó durante unas horas.** Daba
  1–4 DIG, 5 SCL, 6 SDA, 15 GND, 16 VDD. El pinout real es 2 SCL, 3 SDA, 4 GND,
  10 VDD, y lo desmintió la medición de continuidad del 2026-08-02. Sirve para
  el rango de alimentación (2.8–5.5 V) y poco más.
  **Lección:** una tabla de pinout de un agregador no es el datasheet. Antes de
  usar un pinout para decidir dónde soldar, contrastarlo contra la placa real.
- [maxint-rd/TM16xx — TM1650.cpp](https://github.com/maxint-rd/TM16xx/blob/master/src/TM1650.cpp)
  — **la fuente clave**. Documenta el formato del byte de teclado bit a bit
  (bit 6 = pulsada, bits 5–3 columna, bit 2 fijo a 1, bits 1–0 fila), el rango
  válido `0x44`–`0x77`, y que en reposo el valor cae por debajo de `0x40`
  (se ven `0x04`, `0x0C`, `0x2E`). De aquí sale [ADR-011](DECISIONS.md).
- [arkhipenko/TM1650](https://github.com/arkhipenko/TM1650) — confirma las
  direcciones: base de display `0x34 << 1 = 0x68`, control `0x24 << 1 = 0x48`.
- [Tasmota — TM1650](https://tasmota.github.io/docs/TM1650/) y
  [mbed TM1650](https://os.mbed.com/components/TM1637-LED-controller-32-LEDs-max-Keyboa/)
  — confirman que el protocolo es "tipo I2C pero **sin direcciones**, con
  comandos". Respaldan [ADR-006](DECISIONS.md).
- [alldatasheet — AIP650/TM1650 (I-CORE)](https://www.alldatasheet.com/link/datasheet.jsp?p=AIP650/TM1650&m=I-CORE)
  — identifica al fabricante del clon.
- [Ingeniería inversa del HD2015 comparada con el TM1650 (elektroda)](https://www.elektroda.com/rtvforum/topic4052946.html)
  — método de trabajo aplicable: cómo se decodifica un chip de esta familia
  escuchando el bus.

## Escritorios Jiecang — trabajo previo de la comunidad

Aplica **solo si la caja de control tiene un puerto serie adicional**. Nuestro
mando es de los "tontos" (TM1650), lo que apunta a caja de gama sencilla, pero
conviene mirar antes de soldar nada. Ver punto C de [PLAN.md](PLAN.md).

- [phord/Jarvis](https://github.com/phord/Jarvis) — el trabajo de referencia
  sobre el protocolo serie de las cajas Jiecang: 9600 8N1, tramas de 6–9 bytes
  con checksum, la caja emite `0xF2 0xF2` y el mando `0xF1 0xF1`, fin de
  mensaje `0x7E`.
- [Rocka84/jiecang_desk_controller](https://github.com/Rocka84/jiecang_desk_controller)
  — implementación del mismo protocolo.
- [hamxiaoz/jarvis-desk-controller](https://github.com/hamxiaoz/jarvis-desk-controller)
  — ESPHome. Interesante porque hace **exactamente la arquitectura híbrida que
  aquí se eligió**: escucha la telemetría de altura y acciona cortocircuitando
  las líneas de los botones contra GND. Confirma que el enfoque es sensato.
- [Hilo de Home Assistant: Desky / Uplift / Jiecang / Assmann](https://community.home-assistant.io/t/desky-standing-desk-esphome-works-with-desky-uplift-jiecang-assmann-others/383790)
  — hilo largo con muchas variantes de caja. Dato práctico útil: alguien tuvo
  que bajar el *throttle* de altura de 200 ms a 4 ms para que Home Assistant
  no se desincronizara del display. Sirve de aviso sobre la latencia.
- [Catálogo de mandos Jiecang](https://www.jiecang.com/product/jcht35k10.html)
  — sus mandos oficiales son `JCHT35Kxx`. `JK-CH506` no está en catálogo.

---

**Nota sobre estas fuentes:** todas describen el TM1650 desde el lado del
*master* — código que maneja un display. Aquí el papel es el contrario: se
escucha a un master ajeno hablando con un esclavo. Los bytes son los mismos, el
punto de vista no. Ninguna de estas librerías se puede usar tal cual.
