# Brick — premium handheld Brick Game

**Status:** concept / pre-prototype. **Visibility:** private. **Documentation language:** Russian.

An original, premium, rechargeable handheld inspired by classic vertical Brick Game devices. The project emphasizes authentic strictly monochrome pixel graphics, physical controls, high-quality sound, long battery life and upgradeable firmware — without emulating a general-purpose game console.

> [!IMPORTANT]
> This repository is at requirements/discovery stage. No processor, LCD panel, battery, radio-audio profile, PCB or enclosure dimension is approved for manufacture. See `docs/PRODUCT_REQUIREMENTS.md` and `docs/HARDWARE_RESEARCH.md`.

## Confirmed product direction

- Large recognizable **classic Brick Game silhouette**; elegant contemporary execution, not a mini handheld.
- **Strict black-and-white pixel output**, readable with lighting off; optional subtle **white illumination** for darkness (prefer reflective/transflective LCD; not emissive OLED by default).
- D-pad, **one large action button**, **three function buttons**, dedicated physical power switch, and physical wheel/slider volume control (exact control type TBD).
- USB-C **on bottom**, no 3.5 mm audio jack; integrated speaker. Bluetooth is required; whether standard wireless headphones are required is **unresolved** (BLE-only configuration link does not provide conventional BT headphone audio).
- Target **20–30 hours** runtime under a precisely defined test profile; rechargeable battery.
- Launch with approximately **10 classic games**, extend through firmware updates.
- Reuse game rules and regression fixtures from [grayhex/ApolloBrickGames](https://github.com/grayhex/ApolloBrickGames); do not assume its browser/TypeScript code runs unmodified on an MCU.
- Initial manufacturing volume: **100–500 units**. Repository stays private for now.

## Documentation

- [Product requirements](docs/PRODUCT_REQUIREMENTS.md) — authoritative confirmed requirements vs open choices.
- [Hardware research and decision gates](docs/HARDWARE_RESEARCH.md) — LCD, SoC, battery, BLE, sound, production, provisional BOM.
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
2. Resolve whether Bluetooth means **phone configuration over BLE only** or **real headphone audio**.
3. Select 10 launch games and map their controls; check whether one action button is sufficient for every game.
4. Benchmark prospective MCU + LCD + audio power consumption and frame update latency.
5. Prototype physical ergonomics and acoustics **before** defining PCB outline and ordering tooling.

No FPGA is justified solely by classic Brick Game mechanics; assess it only if hardware-faithful reproduction becomes an explicit separate requirement.
