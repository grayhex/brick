# Brick — premium handheld Brick Game

**Status:** concept / pre-prototype. **Visibility:** private. **Documentation language:** Russian.

An original, premium, rechargeable handheld inspired by classic vertical Brick Game devices. The project emphasizes authentic strictly monochrome pixel graphics, physical controls, high-quality sound, long battery life and upgradeable firmware — without emulating a general-purpose game console.

> [!IMPORTANT]
> This repository is at requirements/discovery stage. No processor, LCD panel, battery, radio-audio profile, PCB or enclosure dimension is approved for manufacture. See `docs/PRODUCT_REQUIREMENTS.md` and `docs/HARDWARE_RESEARCH.md`.

## Confirmed product direction

- Large recognizable **classic Brick Game silhouette**; elegant contemporary execution, not a mini handheld.
- **CONFIRMED positive monochrome LCD appearance:** dark/black pixels on a light neutral-gray reflective background, legible without powered illumination in adequate ambient light; optional subtle **white illumination** only for darkness. No reverse/negative LCD theme, greyscale or color UI required. Panel technology and illuminator are still unselected.
- D-pad, **one large action button**, **three function buttons**, dedicated physical power switch, and physical wheel/slider volume control (exact control type TBD).
- USB-C **on bottom**, no 3.5 mm audio jack; integrated speaker. **BLE is required for phone connection, settings and synchronization. All audio is through the built-in speaker; Bluetooth headphones are not supported or required.**
- Target **20–30 hours** runtime under a precisely defined test profile; rechargeable battery.
- Launch with approximately **10 classic games**, extend through firmware updates.
- Reuse game rules and regression fixtures from [grayhex/ApolloBrickGames](https://github.com/grayhex/ApolloBrickGames); do not assume its browser/TypeScript code runs unmodified on an MCU.
- Initial manufacturing volume: **100–500 units**. Repository stays private for now.

## Documentation

- [Product requirements](docs/PRODUCT_REQUIREMENTS.md) — authoritative confirmed requirements vs open choices.
- [Hardware research and decision gates](docs/HARDWARE_RESEARCH.md) — LCD, SoC, battery, BLE, sound, production, provisional BOM.
- [Display shortlist and physical pixel fit](docs/DISPLAY_CANDIDATES.md) — concrete Sharp MIP displays, Azumo frontlight, 10×20+right-side HUD fit, sourcing risks.
- [Game portability plan](docs/GAME_PORTING.md) — factual source mapping and safe migration.
- [Decision log](docs/DECISIONS.md) — status and rationale.
- [Agent instructions](AGENTS.md) — constraints and first execution steps.

## Planned repository layout

```text
brick/
├── README.md
├── AGENTS.md
├── docs/
│   ├── PRODUCT_REQUIREMENTS.md
│   ├── HARDWARE_RESEARCH.md
│   ├── GAME_PORTING.md
│   └── DECISIONS.md
├── design/          # reference mockups, licensed assets, UI
├── hardware/        # KiCad schematic, PCB, BOM and fabrication outputs
├── mechanical/      # CAD, enclosure, buttons, STEP
├── firmware/        # bootloader, platform HAL, games, audio, menu
├── tests/           # reusable gameplay fixtures and HIL tests
└── manufacturing/   # assembly/test instructions, DFM, QA
```

Directories marked as planned do not exist until implementation begins.

## Immediate priorities

1. Validate two monochrome reflective-display families **with physical samples**, including white illumination and visibility with light off.
2. Prototype BLE connection to phone for configuration and sync; no wireless audio streaming.
3. Select 10 launch games and map their controls; check whether one action button is sufficient for every game.
4. Benchmark prospective MCU + LCD + audio power consumption and frame update latency.
5. Prototype physical ergonomics and acoustics **before** defining PCB outline and ordering tooling.

No FPGA is justified solely by classic Brick Game mechanics; assess it only if hardware-faithful reproduction becomes an explicit separate requirement.
