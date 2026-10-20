# Reference repositories — open-source handheld consoles

Checked: 2026-10-20. **Use as structure/checklist inspiration, not as approved BOM, hardware design or permission to copy proprietary assets.** The source repos evolve; links point to `main` or each repo's default branch unless the release commit is pinned later.

## Recommended references by lifecycle stage

| Repo | Verifyable contents in its Git tree | Best use for Brick | Not included / caution |
| --- | --- | --- | --- |
| [jackw01/pi-tin](https://github.com/jackw01/pi-tin) | `3d/` STEP and STL archive, `pcb_main_r2/` and `pcb_display_r2a/` KiCad schematic/board + Gerbers + interactive BOM, `hardware_build.md`, `3dp_assembly.md`, `software_setup.md` | **Most complete practical prototype + assembly layout**; revision-split PCBs, mechanical fit, documentation | Raspberry Pi Linux instead of low-power MCU; do not adopt its power budget |
| [TUDSSL/ENGAGE](https://github.com/TUDSSL/ENGAGE) | `enclosure/*.stl`; `hardware/main-board/` and `hardware/apollo-3-module/` KiCad + schematics PDF + BOM CSV + Gerber ZIP; `software/` firmware + libraries | Co-design and integration of a custom MCU board, screen, power path and console shell in one repo | Research platform for battery-free gaming; not a production-ready turnkey MCU/BLE example |
| [Arduboy/Kit](https://github.com/Arduboy/Kit) | `3d Files/*.stl`; `Eagle Board Files/*.brd/.sch`; `Gerber Board Files/`; `Bill of Materials/*.xls`; `Instructions.html/.pdf`; web flasher and firmware **binaries** | **Small-run build kit and distribution package**, assembly, BOM, re-flashing and reproducible PCB order | Project ROM/game firmware *source* is not all co-located here; inspect separate Arduboy software repos. README mentions STEP but current tree lists STL files only. |
| [Bulat-Mukhametdinov/STM-boy](https://github.com/Bulat-Mukhametdinov/STM-boy) | `hardware/pcb/` BOM, Gerber ZIP, native source + schematic PDF; `hardware/mechanical/` PCB STEP; `hardware/breadboard/`; `software/stm32-bringup/`, `software/attiny13/`; `docs/open-hardware-checklist.md`, `docs/bring-up-log-v0.1.md` | **Audit checklist, hardware bring-up, revision management, production deliverables** | Project still has TODOs in assembly / enclosure / release; check its own documented gaps |
| [sy2002/TeleBall](https://github.com/sy2002/TeleBall) | `cad/` STL, `eagle/` schematic/PCB, `src/TeleBall/TeleBall.ino`, `doc/` illustrated builds/BOM | Small all-in-one DIY embedded handheld layout | Very old AVR/LED console, not USB-C/BLE/modern DFM reference |
| [TinyCircuits/ASM4008-TinyCircuits-Thumby](https://github.com/TinyCircuits/ASM4008-TinyCircuits-Thumby) | `design_files/` Eagle PCB schematic files, `res/Thumby.stl` + ZIP, hardware revision notices | Revisions of an actually sold, tiny single-board gaming product | Games/firmware are in [TinyCircuits-Thumby-Games](https://github.com/TinyCircuits/TinyCircuits-Thumby-Games) and other repos; **not** a single complete source repo |
| [flipperdevices/flipperzero-firmware](https://github.com/flipperdevices/flipperzero-firmware) | `targets/` platform HAL, `applications/`, `furi/` services, `assets/`, `scripts/`, update/recovery docs and debug/factory test apps | Strong benchmark for USB recovery, modular firmware, BLE/USB services, factory diagnostics and release tooling | Not a game-console package, and not a single place for all Flipper hardware CAD |

### Direct links to particularly useful files

- [Pi Tin STEP models](https://github.com/jackw01/pi-tin/tree/master/3d), [main PCB](https://github.com/jackw01/pi-tin/tree/master/pcb_main_r2), [display PCB](https://github.com/jackw01/pi-tin/tree/master/pcb_display_r2a), [printed case assembly](https://github.com/jackw01/pi-tin/blob/master/3dp_assembly.md)
- [ENGAGE enclosure](https://github.com/TUDSSL/ENGAGE/tree/master/enclosure), [ENGAGE board](https://github.com/TUDSSL/ENGAGE/tree/master/hardware/main-board)
- [Arduboy Kit bill of materials](https://github.com/Arduboy/Kit/tree/main/Bill%20of%20Materials), [fabrication outputs](https://github.com/Arduboy/Kit/tree/main/Gerber%20Board%20Files), [assembly instructions](https://github.com/Arduboy/Kit/blob/main/Instructions.pdf)
- [STM-boy release checklist](https://github.com/Bulat-Mukhametdinov/STM-boy/blob/main/docs/open-hardware-checklist.md), [hardware bring-up](https://github.com/Bulat-Mukhametdinov/STM-boy/blob/main/docs/bring-up-log-v0.1.md)
- [Flipper Zero factory debug applications](https://github.com/flipperdevices/flipperzero-firmware/tree/dev/applications/debug) and [build/flash/update guide](https://github.com/flipperdevices/flipperzero-firmware/blob/dev/documentation/fbt.md)

## Licensing / source hygiene

**Do not copy any external repo wholesale into private Brick.** Directory ideas and engineering methods can be adopted independently; copying source, artwork, electronics layout, STEP models, photos, code or bundled libraries requires a separate rights review, preserving upstream attribution and meeting relevant license conditions.

Licenses checked at repo level on 2026-10-20:
- Pi Tin: hardware/3D under **CERN-OHL-W-2.0**; software stated public domain, documentation CC BY-SA 4.0.
- ENGAGE: root LICENSE MIT (verify embedded third-party code separately).
- Arduboy/Kit: root LICENSE MIT; hardware and third-party game/software may have separate constraints.
- STM-boy: split licensing: **CERN-OHL-S-2.0 hardware**, MIT firmware, CC BY 4.0 docs/media (read `LICENSE.md`).
- TeleBall: **CC BY 4.0** in `LICENSE.MD`.
- Thumby design: README says CC BY-SA 3.0; verify file licenses separately.

Maintain Brick's `THIRD_PARTY.md` + future provenance/asset attribution records. Current private status does **not** imply that all third-party resources are free of licensing obligations; decide Brick's own hardware/software licenses before public release, rather than inserting random LICENSE text now.

## Adopt into Brick, without migrating or disrupting the current working software core

- Keep existing `firmware/`, `tests/`, `tools/`, `docs/`, `design/`, `AGENTS.md` and CMake.
- `mechanical/` (CAD master/scripts, measured assembly, STEP/STL prototype, drawings), based on Pi Tin's physical designs and documented fit.
- `hardware/` (KiCad source, symbol/footprint library, schematics, native PCB, BOM, fab outputs, power/display/BLE/USB work), based on ENGAGE and STM-boy.
- `manufacturing/` (assembly maps, SMT pick-and-place, test jig, calibration, QA serial tracking), inspired by Arduboy Kit / STM-boy.
- `releases/` (exact-revision artifacts, source SHA, binary firmware, signatures/hashes, changelog, known issues).
- `docs/` (spec, requirements traceability, firmware/hardware/mechanical pinout, bring-up, measured battery tests, USB recovery, BLE pairing security, regulatory/distribution).
- `companion/` when BLE phone application actually begins (do not create a fake app merely to fill a folder).

**Do not generate a blank directory hierarchy just to satisfy this list.** Check each step against [RELEASE_COMPLETENESS_CHECKLIST.md](RELEASE_COMPLETENESS_CHECKLIST.md) and link actual files in the release only once tested.

## Revision discipline

Three separate version axes:
1. **PCB assembly revision** e.g. H0.1 (native schematic/PCB/BOM/PnP locked per run);
2. **Mechanical revision** e.g. M0.2 (CAD, STEP, gasket, button material and tested PCB fit);
3. **Firmware version** e.g. 0.1.0 (bootloader, app, BLE schema, minimum supported hardware revision).

Capture all three in a device build record and, eventually, serial number. A polished render is *not* evidence of mechanical fit; a successful unit test is *not* evidence of an electrically valid PCB.
