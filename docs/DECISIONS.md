# Architecture Decision Log

Use statuses **CONFIRMED**, **TARGET**, **PROPOSED**, **OPEN**, **REJECTED**. Never present a proposal as an approved part selection.

| ID | Date | Status | Decision / rationale |
| --- | --- | --- | --- |
| ADR-001 | 2026-10-19 | CONFIRMED | Separate private GitHub repo `grayhex/brick`; do not mix embedded hardware BOM/firmware with the web repo. |
| ADR-002 | 2026-10-19 | CONFIRMED | Large original Brick Game form factor; exact CAD dimensions pending. |
| ADR-003 | 2026-10-19 | CONFIRMED | **Black/white only**, optional subtle white illumination for darkness; no color/UI palettes. Supersedes earlier **color AMOLED** concept. |
| ADR-004 | 2026-10-19 | PROPOSED | Evaluate reflective Memory LCD with frontlight and transflective/FSTN with integrated white backlight; select by measured optical/electrical performance and supplier constraints. |
| ADR-005 | 2026-10-19 | CONFIRMED | D-pad + one big Action + 3 function keys + physical On/Off + physical volume wheel or slider. |
| ADR-006 | 2026-10-19 | CONFIRMED | USB-C on bottom, integrated loudspeaker, **no 3.5 mm audio jack**. |
| ADR-007 | 2026-10-19 | CONFIRMED | **Bluetooth LE only** for phone connection, settings and sync. All audio only through built-in speaker; no Bluetooth headphones, A2DP or LE Audio required. |
| ADR-008 | 2026-10-19 | TARGET | 20–30 hours gameplay battery life, measured under explicit test conditions, not theoretical promise. |
| ADR-009 | 2026-10-19 | CONFIRMED | ~10 classic launch games; later games via firmware update; port ApolloBrickGames rules/test fixtures. |
| ADR-010 | 2026-10-19 | CONFIRMED | Small first manufacturing batch: 100–500. |
| ADR-011 | 2026-10-19 | PROPOSED | nRF52840 BLE MCU module now preferred for BLE-only + speaker requirements; final MCU approval depends on display interface, power benchmarks, firmware footprint and availability. |
| ADR-012 | 2026-10-19 | PROPOSED | Avoid FPGA in first hardware revision unless a distinct hardware-exact reproduction requirement appears. |
| ADR-013 | 2026-10-19 | OPEN | microSD slot: on side vs omit; additional games may use onboard Flash. |
| ADR-014 | 2026-10-19 | OPEN | Case finish, manufacturing process, retail price/BOM ceiling, 3 function-key assignments, exact 10 games. |

## Design rule

Every candidate chip/screen/charger in docs is **research only** until a reviewed schematic or recorded measurement and a supplier-supported part number justify selection.

## Next decision checkpoint

Bluetooth requirements now resolved. Next: confirm display samples and enclosure details, then test LCD optical characteristics, frontlight and platform power consumption before MCU/PCB freeze.
