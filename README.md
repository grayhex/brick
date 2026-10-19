# Brick — premium handheld Brick Game

**Status:** concept / pre-prototype. **Visibility:** private. **Documentation language:** Russian.

An original, premium, rechargeable handheld inspired by classic vertical Brick Game devices. The project emphasizes authentic strictly monochrome pixel graphics, physical controls, high-quality sound, long battery life and upgradeable firmware — without emulating a general-purpose game console.

> [!IMPORTANT]
> This repository is at requirements/discovery stage. No processor, LCD panel, battery, radio-audio profile, PCB or enclosure dimension is approved for manufacture. See `docs/PRODUCT_REQUIREMENTS.md` and `docs/HARDWARE_RESEARCH.md`.

## Confirmed product direction

- Large recognizable **classic Brick Game silhouette**; elegant contemporary execution, not a mini handheld.
- **CONFIRMED positive monochrome LCD appearance:** dark/black pixels on a light neutral-gray reflective background, legible without powered illumination in adequate ambient light; optional subtle **white illumination** only for darkness. No reverse/negative LCD theme, greyscale or color UI required. Panel technology and illuminator are still unselected.
- **Four separate small yellow round directional buttons**, **one large smooth yellow action button**, and **three labeled yellow function buttons directly BELOW the LCD (MODE / SELECT / START)**, plus physical power and volume wheel/slider.
- USB-C **centered on the bottom as its only opening**, no 3.5 mm audio jack; **single speaker grille on FRONT near bottom center**. **BLE is required for phone connection, settings and synchronization. All audio is through the built-in speaker; Bluetooth headphones are not supported or required.**
- Target **20–30 hours** runtime under a precisely defined test profile; rechargeable battery.
- Launch with approximately **10 classic games**, extend through firmware updates.
- Reuse game rules and regression fixtures from [grayhex/ApolloBrickGames](https://github.com/grayhex/ApolloBrickGames); do not assume its browser/TypeScript code runs unmodified on an MCU.
- Initial manufacturing volume: **100–500 units**. Repository stays private for now.

## Documentation

- [Product requirements](docs/PRODUCT_REQUIREMENTS.md) — authoritative confirmed requirements vs open choices.
- [Hardware research and decision gates](docs/HARDWARE_RESEARCH.md) — LCD, SoC, battery, BLE, sound, production, provisional BOM.
- [Display shortlist and physical pixel fit](docs/DISPLAY_CANDIDATES.md) — concrete Sharp MIP displays, Azumo frontlight, 10×20+right-side HUD fit, sourcing risks.
- [Enclosure R0.2 concept](docs/MECHANICAL_CONCEPT.md) — preferred classic straight-sided silhouette, 175×80 mm, proposed 20–22 mm thickness, corrected 8 buttons and front speaker.
- [CAD agent handoff prompt](docs/CAD_HANDOFF_PROMPT.md) — ready-to-use editable parametric CAD task, part hierarchy, fit checks, STEP/STL exports.
- [R0.2 three finish variants](design/concepts/brick-family-r02.svg) · [orthographic views](design/concepts/brick-orthographic-r02.svg) · [exploded concept](design/concepts/brick-exploded-r02.svg) — new SVG images with **current** controls.
- [Older R0.1 drawing](design/brick-envelope-r01.svg) — **ARCHIVED**, not the approved button placement.
- [Game portability plan](docs/GAME_PORTING.md) — factual source mapping and safe migration.
- [Decision log](docs/DECISIONS.md) — status and rationale.
- [Agent instructions](AGENTS.md) — constraints and first execution steps.

## Visual reference — Brick R0.2

The following pictures are **vector concept drawings**, not photorealistic renderings or fabrication CAD. They encode the corrected controls and are kept directly in this repository. The user's preferred **original photorealistic triptych** sets the proportions and silhouette; its older D-pad / low function buttons / side-bottom speaker holes are superseded by R0.2 requirements.

**Preferred original silhouette:** [locally archived JPG thumbnail](design/reference-original/preferred-render-thumb.jpg) ([reference notes](design/reference-original/README.md)). The complete high-resolution source PNG still needs binary upload; this archived thumbnail is only 150×137 px.

![User-preferred original visual proportions — preview only](design/reference-original/preferred-render-thumb.jpg)

![Three finishes of Brick R0.2](design/concepts/brick-family-r02.svg)

![Orthographic layout and USB-C-only bottom](design/concepts/brick-orthographic-r02.svg)

![Exploded conceptual component layout](design/concepts/brick-exploded-r02.svg)

**Use for CAD:** [docs/CAD_HANDOFF_PROMPT.md](docs/CAD_HANDOFF_PROMPT.md). Do not infer real PCB routing, RF design, battery capacity, speaker acoustics, or FPC geometry from these visualizations.

## Planned repository layout

```text
brick/
├── README.md
├── AGENTS.md
├── docs/
│   ├── PRODUCT_REQUIREMENTS.md
│   ├── HARDWARE_RESEARCH.md
│   ├── CAD_HANDOFF_PROMPT.md
│   ├── GAME_PORTING.md
│   └── DECISIONS.md
├── design/          # concept SVG images and original-photo thumbnail
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
5. Use current R0.2 drawings + CAD handoff to prototype ergonomics/acoustics **before** defining PCB outline and ordering tooling.

No FPGA is justified solely by classic Brick Game mechanics; assess it only if hardware-faithful reproduction becomes an explicit separate requirement.
