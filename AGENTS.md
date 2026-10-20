# Instructions for coding and engineering agents

Read `README.md`, `docs/REPOSITORY_STRUCTURE.md`, `docs/REFERENCE_PROJECTS.md`, `docs/RELEASE_COMPLETENESS_CHECKLIST.md`, `docs/PRODUCT_REQUIREMENTS.md`, `docs/HARDWARE_RESEARCH.md`, `docs/DISPLAY_CANDIDATES.md`, `docs/MECHANICAL_CONCEPT.md`, `docs/CAD_HANDOFF_PROMPT.md`, `docs/CAD_WORKFLOW.md`, `docs/ENGINEERING_REVIEW_2026-10-20.md`, `docs/GAME_PORTING.md` and `docs/DECISIONS.md` before proposing PCB/firmware work.

## Source of truth
- User-confirmed choices have status **CONFIRMED** in the PRD. Preserve them unless explicitly changed.
- Hardware/model/price assumptions have status **CANDIDATE / UNVERIFIED**, not approved. Do not quietly turn them into fixed requirements.
- Whenever a decision is proposed or accepted, update `docs/DECISIONS.md` with rationale, tests, links and remaining risks.
- Product documents in Russian; code names and identifier conventions may be English.

## Hard constraints
- Strict monochrome logical framebuffer (1 bit/pixel or equivalent two-level cell encoding). Greyscale, RGB color palettes and emissive OLED are **not product requirements**.
- **CONFIRMED positive display polarity:** BLACK/dark active pixels on a light neutral gray/white reflective background; unlit readability in ambient light. Do not invert into a negative display (light pixels on black). Panel type and true contrast remain to be measured.
- **CONFIRMED display composition:** 10 columns × 20 rows for the gameplay matrix, with dedicated auxiliary status/score panel **on the right**, preserving original Brick Game proportions. These are logical cells, not native LCD pixels. Keep layout/rendering independent from the final LCD resolution; do not distort cell aspect ratio or move status beneath the playfield by default.
- Illumination must be optional and subtle white; evaluate display readability with light fully off. A reflective-only LCD generally needs a separately engineered FRONTLIGHT; a transflective LCD may accept a BACKLIGHT. Do not assume lighting is included in a panel.
- Characteristic large vertical Brick Game enclosure; USB-C bottom; no 3.5 mm jack.
- **CURRENT R0.2 control spec overrides R0.1:** four **separate smaller YELLOW ROUND direction keys** (not a D-pad cross, with generous clearance); one **large smooth yellow Action**; **three yellow function keys MODE/SELECT/START directly UNDER the LCD**; dedicated side power switch and volume wheel/slider. Keep them away from palm-contact zone.
- Speaker has **ONE grille on the FRONT near lower center**, no bottom/rear speaker holes. **Only USB-C centered on bottom edge**, no mini-jack or microSD cutout.
- ~10 initial classic games; incremental update support; 20–30 hour measured battery-life goal.
- Target production 100–500. Prioritize parts with supply continuity, accessible datasheets, and manufacturability.

## Game migration
- **Do not** copy the web runtime or browser rendering onto an MCU.
- Existing game rules in `grayhex/ApolloBrickGames` are a reference; code is mostly TypeScript. Check source/third-party licensing before copying assets or code into a future public/commercial release.
- Extract a platform-neutral interface (tick/input/events/cells/save), golden fixtures and deterministic replay, then port selected rules to embedded C/C++ OR test a measured lightweight interpreter approach. Do not assume direct portability.
- Maintain a reproducible cross-language conformance harness with fixed RNG seeds and input traces.
- Some games in the reference repository are approximations, not verified ROM implementations. Clearly label them as such.

## Hardware validation gates
- Compare reflective Memory LCD and transflective monochrome LCD. Screen controller/pinout, viewing angle, illumination method, MOQ, long-term availability and actual measured consumption must be confirmed before PCB freeze.
- **CRITICAL LCD throughput gate:** Sharp LS032B7DD02 documentation quotes `fSCS` 1–10 Hz, SPI max 2 MHz. Clarify with manufacturer and TEST whether it restricts playable full-frame updates to 10 FPS; record 10/15/20/30 FPS screen results before approving this panel. Never assume 60 Hz.
- Battery capacity and hours of operation are goals, not guarantees. Include display, MCU, audio, BLE and regulator losses in power budget.
- CONFIRMED: BLE only for connecting to a phone, settings and synchronization. All sound uses onboard speaker; no Bluetooth headphone streaming, Classic A2DP or LE Audio. Do not introduce dedicated Bluetooth audio hardware.
- Factory test pads, SWD/recovery programming, reverse/ESD protection, battery safety, USB-C CC/power control, DFM, regulatory constraints are mandatory review items.
- Never assert compliance/certification without relevant test records.

- Three finishes share ONE mechanical master: MONOLITH (matte black), CLEAR SMOKE (translucent black, black PCB), CERAMIC (classic warm light gray / beige-gray plastic, ADR-034). **Original photorealistic triptych** is silhouette reference, but its old buttons and speaker outlets are obsolete. **Current SVG**: `design/concepts/*r02.svg`; `design/brick-envelope-r01.svg` is archived and outdated.
- CAD is a **separate task** from firmware/game agent; do not edit `firmware/` or game logic just for mechanical model.

## Repository ownership

- Read [`docs/REPOSITORY_STRUCTURE.md`](docs/REPOSITORY_STRUCTURE.md) for paths and artifact lifecycle. Preserve working `firmware/`, `platform/sim/`, `tests/`, `tools/` and `mechanical/inbox/`; do not move code for aesthetics.
- PCB native KiCad: `hardware/pcb/`; components/BOM/AVL: `hardware/components/`; exports only: `hardware/fabrication/`. Test LCD/interface and perform ERC/DRC/DFM before calling files manufacturing-ready.
- CAD master: `mechanical/src/`; STEP/STL: `mechanical/exports/`. Imported STL in `mechanical/inbox/` is NOT verified master CAD.
- `manufacturing/`, `companion/`, and `releases/` are currently docs/templates, NOT finished factory equipment, companion app or released hardware.
- For meaningful PRs follow [`.github/PULL_REQUEST_TEMPLATE.md`](.github/PULL_REQUEST_TEMPLATE.md) with tests/measurements/known risks. Update [master checklist](docs/RELEASE_COMPLETENESS_CHECKLIST.md) only with evidence.

## Workflow
1. Investigate and summarize alternatives plus measurable acceptance criteria.
2. Propose an ADR (decision) before selecting a part or changing hardware architecture.
3. Prototype/bench test; capture hardware revision, source datasheets, measured results and failure conditions.
4. Advance to KiCad/PCB and manufacturing handoff only after requirements and interface reviews.
5. Use issues/PRs for changes, with screenshots, measurements or test logs as appropriate.

Do not invent supplier stock, prices, battery life or test outcomes.

## Software prototype (firmware/)
- Build and test: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`; also run the ASan/UBSan build (`-DBRICK_SANITIZE=ON`) and the Cortex-M4 cross build (`cmake/arm-none-eabi.cmake`) before proposing changes to `firmware/`.
- The core must stay platform-neutral: no `malloc`, no global mutable state, no vendor headers, no knowledge of a display, panel resolution or board. Hardware access only through `bk_hal_t`.
- Do not change gameplay behaviour of a ported game without regenerating and reviewing the conformance fixtures (`docs/CONFORMANCE.md`); `-ffp-contract=off` is required for bit-exact timing.
- Golden screens (`tests/golden`) change only on purpose: update with `BK_UPDATE_GOLDEN=1`, review the diff, refresh `docs/img`.
- Every new game: pinned reference SHA, fidelity label (verified vs approximation), licence check, conformance traces, ADR; see `docs/FIRMWARE_ARCHITECTURE.md` section 11.
- Keep the logical frame strictly 1 bit, positive polarity, with the status panel to the right (ADR-003/015/016).
- Never assume a 60 Hz display: 60 Hz is the simulation tick. The panel rate is `bk_display_port_t.max_fps`; test playability at 10/15/20/30 FPS on hardware before approving a panel (see the CRITICAL LCD throughput gate above).
- The status column is not fixed to 6.5 cells: fit the compositor to the real panel with `bk_layout_fit` and check legibility in millimetres (`tools/experiments/panel_fit.c`); the frame buffer size is the build option `BRICK_FRAME_MAX_W/H`.
- Physical buttons are the confirmed R0.2 set (four separate direction keys, ACTION, MODE / SELECT / START under the LCD); firmware names are `BK_BTN_*`. What each key does is PROPOSED (ADR-025).
