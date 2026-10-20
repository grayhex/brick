# Brick — Product Requirements Document (PRD), revision 0.6

**Date:** 2026-10-19  
**Stage:** concept and engineering feasibility study  
**Project:** [grayhex/brick](https://github.com/grayhex/brick) (private)  
**Software rules reference:** [grayhex/ApolloBrickGames](https://github.com/grayhex/ApolloBrickGames)

This PRD clearly distinguishes **CONFIRMED** product decisions from **TARGETS** and **OPEN** engineering choices.

## 1. Product vision

Build a standalone premium handheld that preserves the recognisable visual/interaction identity of 1990s Brick Game while upgrading component quality, tactile feel, industrial design, sound, battery, USB-C, maintainable firmware and Bluetooth. The product is for authentic classic pixel games, **not** a general-purpose emulation console.

## 2. Confirmed requirements

| ID | Requirement | Status | Notes |
| --- | --- | --- | --- |
| MECH-01 | Tall, familiar classic Brick Game silhouette, relatively large rather than mini | CONFIRMED | Owner chose **preliminary outer footprint 175×80 mm**; exact CAD tolerances and final size pending physical mockup |
| MECH-02 | Preliminary enclosure thickness **~20–22 mm**, ergonomic acceptance pending | PROPOSED | Azumo frontlight protrusions and LiPo packaging to be included; see `MECHANICAL_CONCEPT.md` |
| DISP-01 | **Strict black/white** gameplay and UI | CONFIRMED | No color palette required |
| DISP-02 | Subtle **white lighting only for darkness**, user switchable | CONFIRMED | Daylight legibility without lighting is intended; LCD technology must be validated |
| DISP-03 | **Positive monochrome LCD**: dark/black active pixels on a light neutral-gray / near-white background | CONFIRMED | User explicitly chose traditional light LCD with black cells, not reverse/negative display; optical white is a visual reference, not a specific material chromaticity |
| DISP-04 | **Classic layout: main playfield 10 columns × 20 rows and a separate information/status zone on its RIGHT** | CONFIRMED | Preserve classic relative proportions, no gameplay cropping or distortion; sidebar for score, level, mode, status; whole panel dimensions/pixel resolution OPEN |
| CTRL-01 | **FOUR separate yellow circular directional buttons** (no unified cross), smaller and visibly spaced to reduce combined accidental presses | CONFIRMED | final diameter/spacing pending ergonomic mockup |
| CTRL-02 | **One** prominent large **smooth yellow circular** action button | CONFIRMED | No legend, symbol or secondary action button |
| CTRL-03 | **Three yellow round functional keys BELOW LCD** labeled **MODE / SELECT / START** | CONFIRMED | exact placement/size pending ergonomic mockup, NOT below gameplay keys |
| CTRL-04 | Separate on/off power control | CONFIRMED | True low-idle/off behavior needed |
| CTRL-05 | Physical **wheel or slider** for volume | CONFIRMED / exact type OPEN | Must be easily adjusted without using menu |
| IO-01 | USB-C on **bottom** edge | CONFIRMED | Data + charging, update/recovery route |
| IO-02 | Do **not** include 3.5 mm analog headphone jack | CONFIRMED | Speaker integrated |
| AUDIO-01 | Built-in high quality sound / loudspeaker, **single grille FRONT at bottom center** | CONFIRMED | no speaker apertures on bottom/sides/rear; acoustic chamber/volume calibration required |
| BT-01 | **Bluetooth Low Energy (BLE)** to connect to phone, configure settings, synchronize data | CONFIRMED | GATT services, security and protocol design OPEN |
| BT-02 | No Bluetooth headphone audio (neither A2DP nor LE Audio); speaker is the **only audio output** | CONFIRMED | No Bluetooth audio transmitter or codec required |
| PWR-01 | USB-C rechargeable battery | CONFIRMED | Chemistry/capacity/vendor OPEN |
| PWR-02 | **20–30 hours gameplay** | TARGET | Define test profile, validate measured whole-device runtime |
| GAMES-01 | Approx. 10 classic games at launch | CONFIRMED | Exact selection OPEN |
| GAMES-02 | Additional games via firmware updates | CONFIRMED | Safe/recoverable update needed |
| GAMES-03 | Reuse logic/rules and tests from ApolloBrickGames | CONFIRMED | Port/reimplement for target platform |
| GAMES-04 | **First firmware version = four games: Tetris, Racing, Snake, Tank** (owner, 2026-10-20, ADR-035); Dice is not to be ported; other candidates only as later firmware updates (GAMES-02) | CONFIRMED | Racing and Tank rules are reconstructions, not ROM-verified (docs/GAME_CANDIDATES.md); the ~10-game target of GAMES-01 stays |
| GAMES-05 | **No game variants: every game ships in ONE variant for now** (owner, 2026-10-20, ADR-036) | CONFIRMED | The 51 Tetris variants, Snake FRAME and the extra Racing road layouts are not ported |
| PROD-01 | Initial production target **100–500 units** | CONFIRMED | Pilot run first |
| REPO-01 | Hardware project in new **private** repository `grayhex/brick` | CONFIRMED | Source repository stays distinct |

## 3. Enclosure layout (intent, not dimensioned CAD)

- Front: large portrait monochrome playfield; immediately below screen three labeled MODE / SELECT / START; below them four *separate* small yellow direction keys (left) and one large smooth yellow action (right); single lower-center **front** loudspeaker grille.
- Side: physical power switch and volume wheel/slider (exact locations OPEN). Optional accessible microSD on side is **not confirmed**.
- Bottom: **only centered USB-C**; **no** mini-jack, no speaker slots/holes or decorative ports. Consider clearances for cable while handheld.
- Rear and bottom must NOT have speaker outlet; front-center lower perforations link to designed acoustic cavity. Avoid hand blockage.
- Finish: **три согласованных исполнения на одной механической платформе** — Monolith (матовый графит), Clear Smoke (дымчатый прозрачный с черной PCB), Ceramic (тёплый светло-серый / бежево-серый пластик в духе классического Brick Game, не керамика; ADR-034). Конкретные материалы и технология производства OPEN.
- Product inspiration includes Analogue Pocket build quality while preserving Brick Game outer shape.

**Current owner's preliminary choice:** approx. **175×80 mm**, see [MECHANICAL_CONCEPT.md](MECHANICAL_CONCEPT.md) and [актуальные R0.2 ортографические виды](../design/concepts/brick-orthographic-r02.svg). Thickness **~20–22 mm** is an engineering proposal, not an owner-approved manufacturing dimension.

## 4. Screen and UI requirements

- Logical screen is monochrome, only two pixel states; positive polarity: dark/black pixels against a light neutral reflective background. Do not infer requirement for RGB, greyscale, or negative black-background / white-pixel theme.
- **Ambient-light readability without active lighting is CONFIRMED**; select panel technology and contrast/reflectivity accordingly. Background may appear neutral-light gray rather than literal pure white.
- White illumination shall be uniform, free of obvious hotspots and independently controllable; reflective LCD requires separately evaluated frontlight/optical stack whereas transflective LCD can use compatible white backlight.
- Menu to preserve classic game letter/number, schematic game preview and mode, plus modern settings.
- **CONFIRMED main gameplay glass layout:** 10 columns × 20 rows of equally proportioned logical cells, with **dedicated information strip to the right** (score, level, modes, status, next piece when relevant). The overall display aspect ratio is wider than the 10:20 playfield alone; exact sidebar width and physical matrix dimensions remain OPEN. Do not treat 10×20 as an exact native pixel resolution.
- Preserve the classic proportions of the gameplay field and auxiliary score/status zone; never stretch cells or let full-screen menus permanently displace the game/status separation.
- Rendering should be **hardware-independent**: define a logical frame/layout and scale using the actual LCD panel constraints after it is chosen; choose square logical cells where compatible with classic source, validate against real device references.
- In case of larger native display resolution, integer scaling, black margins and separate status HUD are acceptable, subject to review.
- Main candidates: reflective Memory LCD with custom frontlight vs monochrome transflective/FSTN with backlight. **E-ink is not default** due to refresh/ghosting challenges; **OLED is emissive and cannot behave as passive LCD**.
- Quantify contrast, outdoor readability, scan/update latency, unit price/MOQ and the optical impact of custom illumination.
- **CRITICAL OPEN ITEM:** Sharp LS032B7DD02 datasheet specifies `fSCS` 1–10 Hz. Its implications for gameplay refresh rate must be tested with representative 10/15/20/30 FPS motion before approval. See [DISPLAY_CANDIDATES.md](DISPLAY_CANDIDATES.md).

## 5. Controls and game compatibility

Target physical controls: four individual yellow round Up/Down/Left/Right keys, one larger smooth yellow Action, three yellow Mode/Select/Start keys **under screen**, Power and analog-feeling volume knob/slider. Logical bindings for games (including Pause/Back/Reset semantics) require per-game mapping and ergonomic testing.

Check reference game quirks before fixing a matrix: some library titles historically expect rotate vs fire and some require both. Consider safe **contextual mapping or long-press alternatives**, not extra physical action buttons, unless user approves a product change.

## 6. Firmware and update design

- Built-in curated game selection, no user-installed code requirement at launch.
- Platform-neutral game interface: fixed ticks, button bitmask, deterministic RNG/state, tile grid output, sound events, settings/save.
- User-friendly USB-C update and recoverable boot path; optional BLE firmware update only after protocol/security review.
- Versioned saves, firmware signatures/checksums, reliable power-failure behavior and regression test matrix.
- Persist records and sound/lighting settings.
- Phone connection via BLE for settings and synchronization is required; app UI/services/security specifics OPEN. USB DFU remains baseline; BLE firmware update optional.

## 7. Target quality acceptance tests (preliminary)

These are TEST PROPOSALS, not demonstrated results:

- 20–30 h measured active gameplay in an agreed brightness/sound/BLE test profile.
- Comfortable visible display in ambient light with white illumination fully disabled.
- Night illumination with no excessive light bleed or hotspots.
- Robust USB update including interrupted-upgrade recovery.
- No false diagonals or stuck buttons in normal use; verify that four separate yellow direction buttons cannot easily be actuated together unintentionally; button feel approved by owner.
- Stable audio without mechanical rattles at intended maximum volume.
- Thermal, battery safety and basic EMC/RF verification before shipment.

## 8. Unresolved product questions

1. What features must the BLE phone companion have at launch beyond settings/sync (e.g. game statistics, saves, firmware update)?
2. Which further classic games complete the ~10-game target after the four of the first firmware version (GAMES-04), and which three labelled function-button roles?
3. Preferred enclosure material/finish: opaque matte, smoked transparent (black PCB visible), or both?
4. Is a microSD slot still desired on the side, or is internal Flash enough for games and updates?
5. What is the acceptable target retail price / max manufactured unit cost? This governs enclosure tooling and LCD sourcing.

- **Mechanical R0.2:** preferred silhouette comes from the user's first Monolith/Clear Smoke/Ceramic photorealistic picture; latest changes override the buttons/speaker of that old picture. New editable vector visualizations and ready CAD prompt at `design/concepts/*r02.svg` and `docs/CAD_HANDOFF_PROMPT.md`.

## 9. Links to concept references

- User's concept sessions (images need to be exported into `design/` before CAD):  
  https://chatgpt.com/s/m_6ad67d651b2081919978c49aa186b155  
  https://chatgpt.com/s/m_6ad67d81491081918785ae607f9d08c0
- Reference logic: https://github.com/grayhex/ApolloBrickGames
- Note: no mockup image files are embedded in this repository yet; do not assume dimensions from those links.
