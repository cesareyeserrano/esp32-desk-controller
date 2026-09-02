# Datasheets

Local copies of the datasheets this project depends on. **They are not
convenience, they are insurance:** external links expire, change URL, or get
protected against downloading. The I-CORE one already returns 403 to any
automated download.

File naming: `manufacturer-part-version.pdf`, all lowercase.

## Contents

### `icore-aip650e-2024-01-b1.pdf` ✅

*AiP650E Product Specification*, Wuxi I-CORE, doc `AiP650E-AX-XS-B037EN`,
version 2024-01-B1, 15 pages. Archived on 2026-08-02.

SHA-256: `4f5b979bad1710de1560a14baafd0444df4e693725bcbdf64beef83cfe28f245`

This is **the project's source of truth**. The verified pinout, the internal
pull-ups that define the probe, the commands, the segment map and the keyboard
code table all come from it. It is cited by [ADR-011](../../DECISIONS.md),
[ADR-012](../../DECISIONS.md), [ADR-013](../../DECISIONS.md) and by the whole of
[PROTOCOLO.md](../../PROTOCOLO.md).

Origin:
`https://www.mikrocontroller.net/attachment/626735/2403051032_Wuxi-I-core-Elec-Aip650EO_C5139014.pdf`

It returns 403 to `curl` and similar tools, so it has to be opened by hand in a
browser. Which is precisely why it lives here.
