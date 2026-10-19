# Instructions for coding and engineering agents

Read `README.md`, `docs/PRODUCT_REQUIREMENTS.md`, `docs/HARDWARE_RESEARCH.md`, `docs/GAME_PORTING.md` and `docs/DECISIONS.md` before proposing PCB/firmware work.

## Source of truth
- User-confirmed choices have status **CONFIRMED** in the PRD. Preserve them unless explicitly changed.
- Hardware/model/price assumptions have status **CANDIDATE / UNVERIFIED**, not approved. Do not quietly turn them into fixed requirements.
- Whenever a decision is proposed or accepted, update `docs/DECISIONS.md` with rationale, tests, links and remaining risks.
- Product documents in Russian; code names and identifier conventions may be English.

## Hard constraints
- Strict monochrome logical framebuffer (1 bit/pixel or equivalent two-level cell encoding). Greyscale, RGB color palettes and emissive OLED are **not product requirements**.
- Illumination must be optional and subtle white; evaluate display readability with light fully off.
- Characteristic large vertical Brick Game enclosure; USB-C bottom; no 3.5 mm jack.
- One D-pad, one large action, three function buttons, dedicated power switch, tactile volume control.
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
- Battery capacity and hours of operation are goals, not guarantees. Include display, MCU, audio, BLE and regulator losses in power budget.
- BLE config/sync and Bluetooth headphone audio are distinct requirements. nRF52840 BLE-only cannot be claimed as standard A2DP or interoperable LE Audio headphone output. Resolve before chip freeze.
- Factory test pads, SWD/recovery programming, reverse/ESD protection, battery safety, USB-C CC/power control, DFM, regulatory constraints are mandatory review items.
- Never assert compliance/certification without relevant test records.

## Workflow
1. Investigate and summarize alternatives plus measurable acceptance criteria.
2. Propose an ADR (decision) before selecting a part or changing hardware architecture.
3. Prototype/bench test; capture hardware revision, source datasheets, measured results and failure conditions.
4. Advance to KiCad/PCB and manufacturing handoff only after requirements and interface reviews.
5. Use issues/PRs for changes, with screenshots, measurements or test logs as appropriate.

Do not invent supplier stock, prices, battery life or test outcomes.
