# Hardware architecture / engineering research (preselection, rev 0.2)

**All candidate parts below are UNVERIFIED** for the final display, supply and 100–500 unit build. Do not treat them as an approved BOM.

## 1. Crucial correction: display technology

Earlier architecture exploration assumed **color AMOLED**. Owner has now confirmed **strict black/white** image with **positive polarity (dark pixels on a light neutral-gray / white-looking reflective field)** and **very subtle optional white lighting for darkness**. Negative-polarity black screens with glowing white pixels are not requested. Therefore the previous AMOLED screen and battery/power estimates are superseded.

AMOLED/OLED pixels emit light and are not passive reflective monochrome panels; they do not provide classic unlit-LCD experience. Instead sample:

| Candidate family | Strength | Engineering risks |
| --- | --- | --- |
| Sharp Memory LCD reflective monochrome (e.g. LS027B7DH01 2.7-inch 400×240 class — confirm exact datasheet/pinout and lifecycle) | High contrast in ambient light, retained image at low update power, pixel detail | Generally NO integrated frontlight: custom white edge/front lighting + optics; interface/VCOM timing; panel aspect/orientation and cost |
| Transflective monochrome FSTN LCD (custom or off-the-shelf, ideally ~2.5–3.5 inch portrait) | Traditional LCD contrast and optional **white backlight**, potentially more authentic | Controller, pixel count, optical performance, MOQ, viewing angles, temperature and custom tooling |
| E-ink/e-paper | Truly unlit monochrome experience | Ghosting, refresh latency, animations; not primary candidate |
| Small monochrome emissive OLED | Simple contrast and rendering | Not visible as intended with illumination off; does not meet passive LCD feel |

**Decision gate:** procure 2–3 physical sample panels, test positive-polarity contrast (black pixel vs light unlit background), outdoor/night legibility, update rate (game dirty rectangles), white lighting uniformity, thickness, integration complexity, availability and volume pricing; then freeze display dimensions/interface **before** MCU/PCB. Reflective Memory LCD typically needs a separate FRONTLIGHT optical solution; transflective LCD can use a compatible white BACKLIGHT. Do not treat either as automatically integrated.

**Display geometry confirmed:** classical 10 columns × 20 rows of game cells plus a **right-side information strip** for score/level/mode/other status. This is not a 10×20 physical pixel panel; whole active area must accommodate the side information zone while preserving game-cell aspect ratio. Compare candidate LCD active-area shape and effective usable pixels accordingly; exact geometry and sidebar width are still open.

## 2. MCU / FPGA candidates

| Candidate | Strengths | Constraints / decision |
| --- | --- | --- |
| **nRF52840 module** (Raytac MDBT50Q family; exact SKU validation pending) | **Preferred candidate:** BLE phone configuration/sync, USB and modest power | 64 MHz Cortex-M4F; 256 KB RAM; verify selected LCD interface, firmware/DFU footprint and power |
| **nRF5340** (module if possible) | Newer dual-core Nordic Bluetooth SoC, more headroom | Complexity not justified by audio (headphones not required); evaluate only if memory/performance needs demand |
| **ESP32-S3** module | Greater CPU/UI headroom, USB and BLE | Wi-Fi unnecessary unless justified; benchmark real sleep and gameplay energy |
| **ESP32 original / ESP32-WROVER** | Classic Bluetooth capability in relevant firmware stacks | Not preferred: headset audio is explicitly out of scope; no reason to add Classic BT hardware |
| **STM32WB55** | Mature MCU with BLE, USB | No automatic standard headphone audio; alternate if STM32 ecosystem preferred |
| **FPGA** (e.g. Lattice ECP5) | Hardware recreation of original logic possible | No benefit to simple custom pixel games, requires BLE/USB/power support chips, RTL development and extra validation; NOT default |

**Default engineering direction, with BLE-only phone connectivity confirmed and display bench still pending:** low-power MCU with integrated BLE, likely nRF52840 module. FPGA is a research option, not a product prerequisite.

## 3. Bluetooth decision (CONFIRMED)

- Only **Bluetooth Low Energy** for phone connection, settings and synchronization.
- All sound uses the **built-in speaker**. No Bluetooth headphones, Classic A2DP, LE Audio, 3.5 mm output or additional wireless-audio transmitter.
- Proposed phone GATT work: read device/firmware info, battery level, write settings, and sync selected game data. Exact schema, authorization and pairing are OPEN.
- USB-C wired firmware recovery/update is baseline; BLE OTA is optional for a later version.
- A low-power integrated-BLE MCU such as the nRF52840 is now the preferred candidate, pending LCD integration, power and firmware validation.

## 4. Preliminary subsystem design

```text
USB-C 5 V ─ ESD/CC ─ power-path charging ─ Li-ion/LiPo 1S with protection + NTC
                         │
                     regulated rails ───────┬── BLE MCU ─ LCD controller / monochrome framebuffer
                                          ├── external Flash (if justified)
                                          ├── buzzer/DAC or I2S class-D ─ speaker + acoustic cavity
                                          ├── D-pad + 1 action + 3 function keys
                                          ├── volume control (wheel or slider; input analog/digital)
                                          └── white LCD illumination (independently switched)
```

USB-C 5V sink without mandatory Power Delivery; correct CC resistors/data path, ESD and protection required. Safe charging with battery temperature management and safe mechanical battery retention. Physical power switch should genuinely isolate/disable high draw domains as designed.

## 5. Candidate BOM (NO procurement approval)

| Block | Candidate / requirement | Decision |
| --- | --- | --- |
| MCU / Bluetooth module | Prefer nRF52840 module for BLE-only requirements; validate LCD/power/firmware | OPEN (SKU not approved) |
| Display | Reflective Memory LCD + custom frontlight **OR** transflective mono LCD + white backlight | OPEN, highest priority |
| Charger / Power Path | TI BQ25619E-class, validate charge current and datasheet | OPEN |
| Battery | 1S Li-Po; 3000–4000 mAh packaging proposal; select capacity based on measured power | OPEN |
| Fuel gauge | MAX17048-class | OPEN |
| Audio | Small efficient amplifier / mono transducer, noise-free envelope and mechanically tuned chamber | OPEN |
| Flash | External QSPI NOR 8–16 MB if needed by OTA and game data | OPEN |
| USB-C | Bottom, 5 V + USB2 data, ESD/CC | CONFIRMED interface, specific parts OPEN |
| Buttons | D-pad, one action, 3 function, separate power, wheel/slider volume | CONFIRMED control topology |
| microSD | Optional side slot | OPEN |
| PCB | likely 4-layer; module antenna clearance and SWD/test pads | PROPOSAL |
| Enclosure | Classic large shape, custom tooling/process per MOQ | CONFIRMED direction; process OPEN |

No supplier MOQ/availability/pricing has been verified. Do not reuse earlier estimated display or unit prices now that the panel class has changed.

## 6. Battery / performance calculation

Battery-life goal: 20–30 hours. Example only: at 1S nominal 3.7 V, 3000 mAh stores 11.1 Wh and 4000 mAh stores 14.8 Wh. With an assumed **85% usable system energy**:

| Battery | 20 h average whole-device ceiling | 30 h average ceiling |
| --- | ---: | ---: |
| 3000 mAh | 472 mW | 315 mW |
| 4000 mAh | 629 mW | 419 mW |

These are hypothetical ceilings, **not** measurements. New monochrome LCD technology may make lower power possible. Measure screen/display rails, frontlight 0% and representative brightness, audio idle/typical/max, MCU game processing, BLE sync and regulator efficiency. Define a repeatable real-world test and power margin.

## 7. Prototype / freeze gates

1. **Display optical review:** white illumination disabled and at night, mechanical FPC, pixel response, supplier MOQ.
2. **BLE phone prototype:** demonstrate pairing, GATT settings/data sync and low-power operation; sound remains on built-in speaker.
3. **Game input mapping:** controls sufficient for all first-wave titles; single action semantics.
4. **Board power prototype:** current consumption in game, idle, backlit/night, BLE and power-off.
5. **Mechanical EVT:** enclosure ergonomic print, battery safe fit, bottom USB clearance, antenna and cavity.
6. **DFM/DVT/PVT:** pinout review, KiCad ERC/DRC, factory SWD/DFU/recovery, test jig, battery and RF/EMC assessments for markets of sale.

## 8. Next research tasks for the engineering agent

- Produce a sourced display comparison of 3–5 purchasable panel modules with dimensional drawings, FPC pinouts, MOQ, price bands **from real supplier quotes** and compatible white-light design.
- Obtain battery/cell mechanical drawings to cross-check full-size enclosure.
- Validate BLE phone GATT behavior/security/power **before approving MCU**.
- Benchmark latency and redraw bandwidth for the selected LCD (use realistic game input/replay data).
- Build quantified per-rail power budget and estimate size/cost range with assumptions separately labelled.
