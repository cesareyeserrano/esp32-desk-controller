# Datasheets

Copias locales de las hojas de datos de las que depende el proyecto. **No son
comodidad, son seguro**: los enlaces externos caducan, cambian de URL o se
protegen contra descarga. El de I-CORE ya devuelve 403 a cualquier descarga
automática.

Nombre de archivo: `fabricante-pieza-version.pdf`, todo en minúsculas.

## Contenido

### `icore-aip650e-2024-01-b1.pdf` ✅

*AiP650E Product Specification*, Wuxi I-CORE, doc `AiP650E-AX-XS-B037EN`,
versión 2024-01-B1, 15 páginas. Archivado el 2026-08-02.

SHA-256: `4f5b979bad1710de1560a14baafd0444df4e693725bcbdf64beef83cfe28f245`

Es **la fuente de verdad del proyecto**. De él salen el pinout verificado, los
pull-ups internos que definen la sonda, los comandos, el mapa de segmentos y la
tabla de códigos de teclado. Lo citan [ADR-011](../../DECISIONS.md),
[ADR-012](../../DECISIONS.md), [ADR-013](../../DECISIONS.md) y todo
[PROTOCOLO.md](../../PROTOCOLO.md).

Origen:
`https://www.mikrocontroller.net/attachment/626735/2403051032_Wuxi-I-core-Elec-Aip650EO_C5139014.pdf`
— devuelve 403 a `curl` y similares; hay que abrirlo a mano en un navegador.
Justo por eso está aquí.
