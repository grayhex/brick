# Brick — master completeness checklist for a manufacturable handheld

**Created:** 2026-10-20. **Scope:** premium positive-monochrome Brick Game, 175×80 mm preliminary enclosure, BLE settings/phone, USB-C DFU + charge, speaker only, 20–30 h **target**, 100–500 pilot units.

This checklist is a **product engineering gate**, not a set of claimed achievements. `[x]` means an existing, reviewable artifact in the repository, **not** a tested manufactured product. A checkbox can be checked only with a link to evidence (measurement, report, source file, photo, quote or released revision).

References: [REFERENCE_PROJECTS.md](REFERENCE_PROJECTS.md). Current prototype audit: [ENGINEERING_REVIEW_2026-10-20.md](ENGINEERING_REVIEW_2026-10-20.md). Approved user decisions: [PRODUCT_REQUIREMENTS.md](PRODUCT_REQUIREMENTS.md) and [DECISIONS.md](DECISIONS.md).

## Gate 0 — product definition / design intent

- [x] Product PRD with CONFIRMED / PROPOSED / TARGET / OPEN decisions: `docs/PRODUCT_REQUIREMENTS.md`
- [x] Consolidated decision log `docs/DECISIONS.md`.
- [x] Three enclosure finishes on ONE geometry with 4 discrete yellow direction keys, 1 Action, 3 labels under LCD, speaker front center: `docs/MECHANICAL_CONCEPT.md` and `design/concepts/*r02.svg`.
- [x] Preliminary LCD candidate research and physical pixel fit: `docs/DISPLAY_CANDIDATES.md`.
- [ ] Final list of ~10 launch games and mapping of rotate/fire/drop per title to agreed **eight physical buttons**.
- [ ] Initial target retail price and **max all-in unit cost** including PCB assembly, case, QC, packaging, shipping and reject allowance.
- [ ] Intended sale/ship regions and expected labeling, consumer product, battery and radio regulatory obligations. Select regional compliance test laboratory before PCB freeze.
- [ ] Decide microSD (currently OPEN). Do **not** cut slot on first CAD before an owner decision.
- [ ] Select and document authorized suppliers, alternatives and lifecycle/availability for critical parts.

## Gate 1 — functional design + screen user study (blocks PCB freeze)

- [ ] Test native 3.16-inch Sharp **actual** assembled LCD+frontlight samples (not optical film alone); obtain full drawing and FPC pinout.
- [ ] Resolve `fSCS 1–10Hz` and actual usable full/partial update performance under Tetris and fast games.
- [ ] Physical 1:1 readability test: 10×20 main field, **right status column**, NEXT, SCORE, labels at normal playing distance. Existing native 3.16-inch fit makes status column **~9.7mm** and digits **~1×1.9mm**; numerical fit ≠ optical acceptance.
- [ ] Measured contrast, unlit daylight visibility, reflection/glare, eye-level angle and night **white light** uniformity without hotspots.
- [ ] Side-by-side 3.16-inch vs 4.4-inch/other LCD if 3.16-inch fails rate/readability; preserve agreed 175×80 envelope unless change approved.
- [ ] Approve LCD model, revision, availability, lead time, purchase MOQ and landed cost for 100/500 units.
- [ ] Confirm expected requirements for temperature, shock, sunlight, service lifetime and warranty.

## Gate 2 — mechanical CAD + physical ergonomics

- [x] Draft coordinate/button and case CAD agent handoff: `docs/CAD_HANDOFF_PROMPT.md`, `docs/CAD_WORKFLOW.md`.
- [ ] Reproducible parameterized master CAD source (e.g. `mechanical/src/`, native editable files + version/tool chain), **not only STL/STEP images**.
- [ ] Identifiable independent parts: two case shells, display bezel/lens, eight buttons + switch parts, speaker port/cavity, PCB and battery fit models.
- [ ] Assembly `*.step`, STL/3MF for printable prototypes, orthographic/mechanical drawings, section cuts, exploded BOM and clearance dimensions.
- [ ] One shared case geometry across Monolith/Clear Smoke/Ceramic with explicit material and appearance differences.
- [ ] 1:1 printed ergonomic prototype test for grip, fingertip travel and zero palm activation; four direction keys must be separated.
- [ ] Validate bottom edge **only USB-C**, center-front speaker outlet no duplicate port. Test sound blockage by both hands.
- [ ] Choose actual battery geometry including NTC/PCM, swelling allowance, cable route and insulation; no assumption of guaranteed 4000 mAh.
- [ ] Refit after selection of exact LCD/light/FPC and PCB/antenna to avoid impossible assembly.
- [ ] Define fasteners, access for battery service, screw bosses, seam, tolerances, materials/finishes (smoky transparent PC / opaque PC-ABS alternatives), impact/aging, molding method/tooling feasibility.
- [ ] Produce verified `mechanical/fit-report.md` with interference checks and photographs.

## Gate 3 — electronics architecture and custom PCB

- [ ] Approved block diagram; final MCU/BLE module (currently **nRF52840 PROPOSED**, not selected), PCB memory, audio amp, charger/protection/power rails.
- [ ] Measured current budget and 20–30 h battery discharge profile at representative gameplay/volume/backlight; include active 60Hz core, panel refresh, BLE and deep sleep currents.
- [ ] USB-C connector design incl. CC resistors, charging/USB data, ESD/overvoltage, status/protection and recovery pinout.
- [ ] Electrical power path for charging while using device, battery protection, thermal supervision/NTC, low-battery behavior, safe hard-off and user power switch.
- [ ] Display voltages/level conversion/interface and FPC footprint confirmed for exact panel+frontlight; voltage and VCOM/inversion verified.
- [ ] Audio topology with amplifier, proper load speaker, PWM/DAC/filtered waveform and **physical** volume wheel/slider that satisfies quality/noise.
- [ ] BLE antenna keep-out by chosen module datasheet; battery/LCD/metalization interaction; prototype RF range and detuning.
- [ ] SWD/JTAG pad access and **hardware-enforced unbrick/recovery** path; debug UART/test pads as appropriate.
- [ ] Native `hardware/pcb/` KiCad schematics + PCB source, verified footprints and 3D model, ERC/DRC clean or reviewed/waived.
- [ ] BOM with manufacturer part numbers (MPN), supplier SKU, substitutes, package/footprint, distributor MOQ, cost at 10/100/500, lead time and lifecycle status.
- [ ] Obtain DFM/DFT feedback from PCB assembler **before** locking manufacturing data.
- [ ] Produce fab package: Gerber, drill, board outline, stackup/copper/finish/mask, fab notes, design revision; top/bottom assembly drawings, centroid/pick-place CSV, BOM/DNP list, stencil spec.
- [ ] All hardware files, CAD and assembly drawings belong to the **same revision**.

## Gate 4 — firmware, games, BLE companion

- [x] Portable C11 core, simulation 60Hz, game/scene API, 1bpp renderer and HAL boundaries: `firmware/`.
- [x] Base Tetris, pinned ApolloBrickGames reference, tick-level fixtures and tests: `tests/`, `docs/CONFORMANCE.md`. **Not full set of ten games.**
- [x] Core host CI/compiler/sanitizer and Cortex-M4 cross-compile smoke test: `.github/workflows/ci.yml`. **Not full linked+flashed ARM firmware.**
- [ ] Interactive desktop simulator with physical controls and 10/15/20/30Hz display emulation; measure usability and score legibility.
- [ ] Port selected launch titles with correctness tests and a documented mapping for independent FIRE/ROTATE where applicable.
- [ ] Board-specific startup/linker/SDK, hardware HAL implementations (SPI LCD, buttons debounce, frontlight, audio, flash, power, USB), built and **executed on real hardware**.
- [ ] Measure CPU, peak stack, linker RAM/flash map, GPU/bitmap composition duty cycle, actual FPS, watchdog and low-power runtime.
- [ ] Firmware build produces signed/verified upgrade images where feasible; factory image, bootloader, safe fallback, **recoverable USB-C DFU after interrupted flash** and reproducible flashing instructions.
- [ ] Firmware version + PCB revision + mechanical revision + protocol schema, device identity/serial and compatible update policy.
- [ ] Define BLE GATT data contract for phone settings/status/sync; permissions and authentication/pairing/bonding, replay/unauthorized writes; iOS/Android tests.
- [ ] Phone app implementation or documented supported companion workflow; do not mark BLE requirement complete because HAL has no BLE audio.
- [ ] Settings/high scores persistent storage endurance, brownout/half-written flash behavior, format migrations across firmware updates.
- [ ] Factory diagnostics firmware: each button, display pixel grid, light, sound, battery/charging, BLE advertisement/pairing, USB, flash CRC; produces machine-readable pass/fail.
- [ ] Game assets/sound provenance and redistribution clearance: see `THIRD_PARTY.md`. Do not assume all retro graphics are freely reusable.
- [ ] User-facing instructions for reset, recovery and upgrading games via supported firmware.

## Gate 5 — EVT → DVT → PVT (actual builds)

- [ ] EVT: 2–5 engineering units, bring-up log with voltages, USB charging, current measurements, display bring-up, loudspeaker and radio.
- [ ] DVT: ergonomic/prototype fit, key life tests, lens scratching, drop/shock, charge while running, thermal and idle/sleep; battery capacity run test.
- [ ] RF/electrical pre-compliance (radiated emissions, intentional radio, ESD) and required region-specific certification/lab reports for selling BLE+LiPo device. Not every market has the same obligations; document actual destinations.
- [ ] PVT: small pilot SMT + case process, serial-number batch, clear work instructions, fixtures, incoming goods acceptance and controlled rework.
- [ ] Repeatable **end-of-line test jig** (USB/SWD pogo-pin access): flash, self-test, button scan, sound/display/light, BLE, charging and sleep-current test. Store log keyed by serial number and hardware revision.
- [ ] Yield/defect/failure reporting, reject/rework criteria, QA sign-off, gold sample for each finish, known-issues and change control.
- [ ] Battery transport/shipping documentation and actual packaging protection, adhesive/isolation/thermal safety.
- [ ] Pilot acceptance criteria and owner signoff prior to ordering 100–500 finished units.

## Gate 6 — release/support/manufacturing archive

- [ ] Rev-specific package in `releases/Hx-My-fwz/` containing fixed **commit hashes**, final KiCad source, schematics PDF, Gerbers/drill, BOM, PnP, assembly drawings, mechanical source/STEP/STL, test procedure, firmware binaries and verification hashes.
- [ ] Supplier quotes/PO part numbers, revisioned approved AVL, cost and lead-time snapshots, fabrication notes and factory contact process stored appropriately (commercial documents may be private).
- [ ] User manual / support FAQ, button mapping, Bluetooth pairing, USB recovery procedure, battery handling, troubleshooting and firmware update instructions.
- [ ] Compliance statements/labels specific to the destination markets and actual results; do not put CE/FCC/EAC graphics on renders without validation.
- [ ] Software/hardware/documentation licensing decisions and obligations, retained copyright notices and rights provenance; current `THIRD_PARTY.md` is a starting point.
- [ ] Warranty, spare parts, repair/service plan, battery replacement path, issue tracker and versioned changelog.
- [ ] Backup release artifact storage with verified checksums; tested ability to recreate a unit of the same HW/FW revision from archived source.

## Priority work queue — what to do before moving to PCB

1. **LCD go/no-go** (frame rate and 1:1 readability) — [issue #5](https://github.com/grayhex/brick/issues/5).
2. **Parameterized printed shell** in common geometry, actual components' fit and speaker/frontlight constraints — [issue #3](https://github.com/grayhex/brick/issues/3).
3. **Interactive gameplay UX** with throttled screen refresh — [issue #4](https://github.com/grayhex/brick/issues/4).
4. Resolve launch-game/FIRE mapping and first-round BOM cost target.
5. Then—and only then—freeze MCU/display choices, FPC and PCB outline.

**Rule for agents:** Treat unchecked items as tasks/risks, not as existing deliverables. Document experimental and completed status separately; do not prematurely change a `[ ]` to `[x]` without a link to reproducible proof or test result.
