# Brick — Product Requirements Document (PRD), revision 0.4

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
| MECH-01 | Tall, familiar classic Brick Game silhouette, relatively large rather than mini | CONFIRMED | Exact size not approved |
| DISP-01 | **Strict black/white** gameplay and UI | CONFIRMED | No color palette required |
| DISP-02 | Subtle **white lighting only for darkness**, user switchable | CONFIRMED | Daylight legibility without lighting is intended; LCD technology must be validated |
| DISP-03 | **Positive monochrome LCD**: dark/black active pixels on a light neutral-gray / near-white background | CONFIRMED | User explicitly chose traditional light LCD with black cells, not reverse/negative display; optical white is a visual reference, not a specific material chromaticity |
| DISP-04 | **Classic layout: main playfield 10 columns × 20 rows and a separate information/status zone on its RIGHT** | CONFIRMED | Preserve classic relative proportions, no gameplay cropping or distortion; sidebar for score, level, mode, status; whole panel dimensions/pixel resolution OPEN |
| CTRL-01 | Physical D-pad | CONFIRMED | Final mechanics under review |
| CTRL-02 | **One** prominent large action button | CONFIRMED | No pair of action buttons |
| CTRL-03 | **Three** dedicated function buttons | CONFIRMED | Meaning/keycaps to be set |
| CTRL-04 | Separate on/off power control | CONFIRMED | True low-idle/off behavior needed |
| CTRL-05 | Physical **wheel or slider** for volume | CONFIRMED / exact type OPEN | Must be easily adjusted without using menu |
| IO-01 | USB-C on **bottom** edge | CONFIRMED | Data + charging, update/recovery route |
| IO-02 | Do **not** include 3.5 mm analog headphone jack | CONFIRMED | Speaker integrated |
| AUDIO-01 | Built-in high quality sound / loudspeaker | CONFIRMED | Acoustic chamber/volume calibration required |
| BT-01 | **Bluetooth Low Energy (BLE)** to connect to phone, configure settings, synchronize data | CONFIRMED | GATT services, security and protocol design OPEN |
| BT-02 | No Bluetooth headphone audio (neither A2DP nor LE Audio); speaker is the **only audio output** | CONFIRMED | No Bluetooth audio transmitter or codec required |
| PWR-01 | USB-C rechargeable battery | CONFIRMED | Chemistry/capacity/vendor OPEN |
| PWR-02 | **20–30 hours gameplay** | TARGET | Define test profile, validate measured whole-device runtime |
| GAMES-01 | Approx. 10 classic games at launch | CONFIRMED | Exact selection OPEN |
| GAMES-02 | Additional games via firmware updates | CONFIRMED | Safe/recoverable update needed |
| GAMES-03 | Reuse logic/rules and tests from ApolloBrickGames | CONFIRMED | Port/reimplement for target platform |
| PROD-01 | Initial production target **100–500 units** | CONFIRMED | Pilot run first |
| REPO-01 | Hardware project in new **private** repository `grayhex/brick` | CONFIRMED | Source repository stays distinct |

## 3. Enclosure layout (intent, not dimensioned CAD)

- Front: large portrait monochrome playfield, D-pad, one large action button, three labelled function buttons.
- Side: physical power switch and volume wheel/slider (exact locations OPEN). Optional accessible microSD on side is **not confirmed**.
- Bottom: USB-C; **no** mini-jack. Consider clearances for a cable while device is handheld or placed on a desk.
- Rear or bottom: speaker outlet connected to designed acoustic cavity. Avoid obstruction when held.
- Finish: premium, smooth mechanical fit; material, transparent shell/black PCB aesthetic, coating and window details OPEN.
- Product inspiration includes Analogue Pocket build quality while preserving Brick Game outer shape.

**Dimension proposal for ergonomics only:** investigate 145–165 mm height and fit of battery/cell/display; not approved.

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

## 5. Controls and game compatibility

Target physical controls: Up/Down/Left/Right, one Action, three Function keys, Power, and analog-feeling volume knob/slider. Function allocation (Start/Pause, Game/Mode, Reset/Back, etc.) requires per-game mapping and ergonomic testing.

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
- No false diagonals or stuck buttons in normal use; button-actuation feel approved by owner.
- Stable audio without mechanical rattles at intended maximum volume.
- Thermal, battery safety and basic EMC/RF verification before shipment.

## 8. Unresolved product questions

1. What features must the BLE phone companion have at launch beyond settings/sync (e.g. game statistics, saves, firmware update)?
2. Which exact classic **10 launch games**, and which three labelled function-button roles?
3. Preferred enclosure material/finish: opaque matte, smoked transparent (black PCB visible), or both?
4. Is a microSD slot still desired on the side, or is internal Flash enough for games and updates?
5. What is the acceptable target retail price / max manufactured unit cost? This governs enclosure tooling and LCD sourcing.

## 9. Links to concept references

- User's concept sessions (images need to be exported into `design/` before CAD):  
  https://chatgpt.com/s/m_6ad67d651b2081919978c49aa186b155  
  https://chatgpt.com/s/m_6ad67d81491081918785ae607f9d08c0
- Reference logic: https://github.com/grayhex/ApolloBrickGames
- Note: no mockup image files are embedded in this repository yet; do not assume dimensions from those links.
