# Brick — premium handheld Brick Game

**Status:** concept / pre-prototype; **first software prototype added** (portable game core, base Tetris, menu — see below). **Visibility:** private. **Documentation language:** Russian.

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

## Программный прототип (0.1)

Переносимое игровое ядро на C11, не привязанное к дисплею, разрешению, MCU или плате (ADR-017…023):

- **Ядро** `firmware/`: игра = POD-структура с таблицей функций, фиксированный такт 60 Гц, детерминированный ГСЧ, нет кучи и глобального состояния; граница с железом — `bk_hal_t` (ввод, дисплей, звук, хранилище, подсветка).
- **Tetris, базовый вариант 01**, перенесён из [ApolloBrickGames](https://github.com/grayhex/ApolloBrickGames) и **сверяется с эталоном на каждом такте**: 68 сценариев, 59 357 тактов ([docs/CONFORMANCE.md](docs/CONFORMANCE.md)). Остальные 50 вариантов и ПК-симулятор — вне этого PR ([docs/GAME_TETRIS.md](docs/GAME_TETRIS.md)).
- **Экран:** 1-битный логический кадр, позитивная полярность, поле 10×20 + панель справа, независимо от LCD ([docs/UI_MENU.md](docs/UI_MENU.md)).
- **Меню:** номер/буква игры, номер режима, миниатюра, SPEED/LEVEL, настройки; D-pad, одна кнопка действия, три функциональные кнопки.
- Размер ядра под Cortex-M4F: ≈ 22 КБ flash, `.data = .bss = 0` (**только компиляция**, на железе не измерялось).

```sh
cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure
cmake -S . -B build-asan -G Ninja -DBRICK_SANITIZE=ON && cmake --build build-asan && ctest --test-dir build-asan   # ASan + UBSan
cmake -S . -B build-m4 -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DCMAKE_BUILD_TYPE=Release -DBRICK_BUILD_TESTS=OFF \
  && cmake --build build-m4 && cmake --build build-m4 --target brickcore_size                                       # проверка переносимости
```

Нужны CMake ≥ 3.16 и компилятор C11/C++17; Node ≥ 20 — только для регенерации фикстур соответствия.

## Documentation

- [Product requirements](docs/PRODUCT_REQUIREMENTS.md) — authoritative confirmed requirements vs open choices.
- [Hardware research and decision gates](docs/HARDWARE_RESEARCH.md) — LCD, SoC, battery, BLE, sound, production, provisional BOM.
- [Game portability plan](docs/GAME_PORTING.md) — factual source mapping and safe migration.
- [Decision log](docs/DECISIONS.md) — status and rationale.
- [Архитектура прошивки](docs/FIRMWARE_ARCHITECTURE.md) — слои, HAL, память, порт на ARM MCU, как добавить игру.
- [Tetris: что портировано](docs/GAME_TETRIS.md), [соответствие эталону](docs/CONFORMANCE.md), [меню и управление](docs/UI_MENU.md).
- [Сторонние материалы](THIRD_PARTY.md) — происхождение и лицензии.
- [Agent instructions](AGENTS.md) — constraints and first execution steps.

## Repository layout

```text
brick/
├── README.md, AGENTS.md, THIRD_PARTY.md
├── CMakeLists.txt, cmake/        # host build and the Cortex-M4F cross-compile smoke test
├── docs/                         # requirements, research, decisions, firmware architecture, UI, conformance
│   └── img/                      # illustrations of the logical frame (generated from tests/golden)
├── firmware/                     # portable core (no board, no display, no MCU)
│   ├── include/brick/            # public API: game, hal, console, render, audio, save, ...
│   ├── core/                     # game base, input, rng, frame, font, render, audio, save, console
│   └── games/tetris/             # base Tetris variant
├── tests/                        # unit, conformance (recorded reference traces), golden screens
│   ├── fixtures/tetris/          # traces exported from the TypeScript reference
│   └── golden/                   # ASCII pictures of every console screen
├── tools/
│   ├── conformance/              # exporter of reference fixtures (Node, only for regeneration)
│   ├── frames/                   # golden frame -> PNG
│   └── experiments/              # reproducible experiments behind ADRs
├── design/          # (planned) reference mockups, licensed assets
├── hardware/        # (planned) KiCad schematic, PCB, BOM and fabrication outputs
├── mechanical/      # (planned) CAD, enclosure, buttons, STEP
└── manufacturing/   # (planned) assembly/test instructions, DFM, QA
```

Directories marked *(planned)* do not exist until implementation begins. PCB work has not started.

## Immediate priorities

1. Validate two monochrome reflective-display families **with physical samples**, including white illumination and visibility with light off.
2. Prototype BLE connection to phone for configuration and sync; no wireless audio streaming.
3. Select 10 launch games and map their controls; check whether one action button is sufficient for every game.
4. Benchmark prospective MCU + LCD + audio power consumption and frame update latency.
5. Prototype physical ergonomics and acoustics **before** defining PCB outline and ordering tooling.

No FPGA is justified solely by classic Brick Game mechanics; assess it only if hardware-faithful reproduction becomes an explicit separate requirement.
