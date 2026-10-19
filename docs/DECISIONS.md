# Architecture Decision Log

Use statuses **CONFIRMED**, **TARGET**, **PROPOSED**, **OPEN**, **REJECTED**. Never present a proposal as an approved part selection.

| ID | Date | Status | Decision / rationale |
| --- | --- | --- | --- |
| ADR-001 | 2026-10-19 | CONFIRMED | Separate private GitHub repo `grayhex/brick`; do not mix embedded hardware BOM/firmware with the web repo. |
| ADR-002 | 2026-10-19 | CONFIRMED | Large original Brick Game form factor; exact CAD dimensions pending. |
| ADR-003 | 2026-10-19 | CONFIRMED | **Black/white only**, optional subtle white illumination for darkness; no color/UI palettes. Supersedes earlier **color AMOLED** concept. |
| ADR-004 | 2026-10-19 | PROPOSED | Evaluate reflective Memory LCD with frontlight and transflective/FSTN with integrated white backlight; select by measured optical/electrical performance and supplier constraints. |
| ADR-015 | 2026-10-19 | CONFIRMED | **Positive LCD polarity**: black/dark pixels on a light neutral gray/white-looking reflective background, readable in ambient light with illumination OFF. No negative-polarity white-on-black display. White night illumination only; exact LCD/optics still OPEN. |
| ADR-016 | 2026-10-19 | CONFIRMED | Maintain original Brick Game screen geometry: **10×20 logical cell playfield plus persistent information/status zone to the RIGHT**. Native pixel resolution, sidebar width and exact overall physical dimensions are OPEN. |
| ADR-017 | 2026-10-20 | PROPOSED | Evaluate **Sharp LS032B7DD02 3.16-inch + Azumo frontlight assembly** first. Strong portrait fit (336×536 -> 10×20 cells of 26 pixels, 76 px for side status). **HIGH RISK: Sharp specifies `fSCS` ≤10 Hz; FPS suitability unconfirmed.** Compare other models / alternatives and test before committing. See `docs/DISPLAY_CANDIDATES.md`. |
| ADR-018 | 2026-10-20 | CONFIRMED / PROPOSED | User chose preliminary outside footprint **175×80 mm** with 3.16-inch Sharp as **candidate**; thickness **~20–22 mm only proposal** pending LiPo/FLP/PCB test; evaluate three case finishes: graphite matte, smoke translucent, warm ivory. Mechanical CAD/DFM not frozen. See `docs/MECHANICAL_CONCEPT.md`. |
| ADR-005 | 2026-10-19 | SUPERSEDED | Original connected D-pad; superseded by ADR-020 / revised 4 separate yellow round directions. |
| ADR-006 | 2026-10-19 | CONFIRMED | USB-C on bottom, integrated loudspeaker, **no 3.5 mm audio jack**. |
| ADR-007 | 2026-10-19 | CONFIRMED | **Bluetooth LE only** for phone connection, settings and sync. All audio only through built-in speaker; no Bluetooth headphones, A2DP or LE Audio required. |
| ADR-008 | 2026-10-19 | TARGET | 20–30 hours gameplay battery life, measured under explicit test conditions, not theoretical promise. |
| ADR-009 | 2026-10-19 | CONFIRMED | ~10 classic launch games; later games via firmware update; port ApolloBrickGames rules/test fixtures. |
| ADR-010 | 2026-10-19 | CONFIRMED | Small first manufacturing batch: 100–500. |
| ADR-011 | 2026-10-19 | PROPOSED | nRF52840 BLE MCU module now preferred for BLE-only + speaker requirements; final MCU approval depends on display interface, power benchmarks, firmware footprint and availability. |
| ADR-012 | 2026-10-19 | PROPOSED | Avoid FPGA in first hardware revision unless a distinct hardware-exact reproduction requirement appears. |
| ADR-013 | 2026-10-19 | OPEN | microSD slot: on side vs omit; additional games may use onboard Flash. |
| ADR-014 | 2026-10-19 | PARTIAL | Three finishes defined and function labels MODE/SELECT/START agreed; manufacturing process, target BOM/price and exact launch games remain open. |

| ADR-019 | 2026-10-20 | CONFIRMED | The **original first photorealistic Monolith / Clear Smoke / Ceramic triptych** is authoritative for long, nearly straight case silhouette/proportions over later short/bent renders. Case approx 175×80 mm; shell thickness 20–22 proposal. However control layout and speaker holes in that early render are OBSOLETE. |
| ADR-020 | 2026-10-20 | CONFIRMED | **4 separate, smaller and spaced yellow round directional buttons**, **1 large smooth yellow action**, **3 small yellow labeled MODE/SELECT/START buttons directly below LCD**. Exact spacing provisional, see R0.2 CAD spec. |
| ADR-021 | 2026-10-20 | CONFIRMED | **Single front speaker grille near lower center**, no speaker hole on bottom or back; **USB-C only on bottom center**. |
| ADR-022 | 2026-10-20 | CONFIRMED | Three executions of **one shared mechanical/electronic platform**: MONOLITH matte graphite, CLEAR SMOKE transparent smoke + black PCB, CERAMIC ivory plastic (not true ceramic). Geometry/controls identical; material shrinkage requires separate DFM. |

## Design rule

Every candidate chip/screen/charger in docs is **research only** until a reviewed schematic or recorded measurement and a supplier-supported part number justify selection.

## Next decision checkpoint

Bluetooth requirements now resolved. Next: confirm display samples and enclosure details, then test LCD optical characteristics, frontlight and platform power consumption before MCU/PCB freeze.
