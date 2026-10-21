# ApolloBrickGames → Brick device: game porting study

**Source of truth:** https://github.com/grayhex/ApolloBrickGames (`main`, audited 2026-10-19). Existing project is **TypeScript + Vite + Canvas 2D + Web Audio**, not embedded C/C++. Hardware firmware requires an explicit port or alternative execution strategy.

## 1. Verified useful paths

| Content | Path in ApolloBrickGames | Reuse approach |
| --- | --- | --- |
| Architectural boundaries, fixed step, deterministic RNG/replay | `docs/architecture.md`, `src/core/` | Reuse specification and golden replay/input traces |
| Brick game registry and aliases | `src/brick/games/index.ts` | Source IDs and metadata, map launch games |
| Tetris | `src/brick/games/tetris.ts`, `src/brick/games/tetris/` | Port rules and validation fixtures |
| Tank | `src/brick/games/tanks.ts`, `src/brick/games/tanks/` | Port grid logic, AI and levels |
| Snake and other classic library games | `src/brick/games/library/` | Inventory, adapt game logic |
| Racing / Frogger / Arkanoid / Invaders | `src/brick/games/racing/`, `frogger/`, `arkanoid.ts`, `invaders.ts` | Potential launch candidates |
| Brick grid and base interfaces | `src/brick/grid.ts`, `src/brick/base.ts` | Embedded-friendly API design |
| Sound event synthesis | `src/audio/brick.ts`, `src/audio/beeper.ts` | Port event maps, not Web Audio runtime |
| Game coverage, reproducible tests | `tests/brick/` | Golden state tests, fixed input/replay fixtures |
| Current game catalogue and authenticity status | `docs/brick/library.md`, `docs/brick/rom-arcade.md`, `docs/reference-status.md` | Select verified titles and mark approximations |
| Third-party content attributions | `docs/THIRD_PARTY.md` | Audit before distributing code/assets |

## 2. Important caveats

- The upstream library documents **55 game/variant records**, not necessarily 55 unique original ROM games. Many entries are playable approximations; selection should explicitly mark provenance and completeness.
- Upstream has nuances: some inputs use ROTATE alias, others truly need FIRE; `jigsaw` needs a separate FIRE control and is not currently assignable to a four-input/rotate-only console. Validate one-action scheme per launch game.
- The upstream README describes game cores independent of DOM and with deterministic ticks. This is an excellent portability boundary, **not** evidence that TypeScript builds natively for nRF.
- Upstream includes third-party materials and CC0-derived art in places. Review attribution and license applicability before publishing derivative embedded assets, product artwork, trademarks, or commercial firmware.
- Web-only UI, Canvas and Web Audio should not be carried onto MCU.

## 3. Proposed embedded interface

```c
typedef uint16_t BrickButtons; // UP/DOWN/LEFT/RIGHT/ACTION/FN1/FN2/FN3

void game_init(uint32_t seed, uint8_t mode);
void game_tick(BrickButtons pressed, BrickButtons held);
const BrickFrame *game_frame(void); // 1-bit cells / logical grid
size_t game_pop_audio_events(BrickAudioEvent *dst, size_t capacity);
bool game_serialize(uint8_t *dst, size_t capacity);
bool game_deserialize(const uint8_t *src, size_t size);
```

This is a **sketch**, not a frozen ABI. Favor scene/cell representations independent of actual LCD pixels; playfield letterboxing/integer scaling belongs to renderer. Define deterministic tick speed per game.

## 4. Conformance workflow

1. Choose approximately 10 named **exact** IDs from upstream game registry.
2. Identify game control mapping and original fidelity level with sources.
3. Execute TypeScript reference games with fixed seed and scripted input frames; save expected state hashes/score/board/time/audio-event sequences in JSON.
4. Implement game loop in C or C++ without platform graphics/audio dependencies.
5. Run the same fixtures against firmware core in native host tests and compare expected results. Verify saved state versions.
6. Add LCD driver/hardware abstraction only after host tests pass.
7. Run on board with timing/power tests and hardware button debouncing.

## 5. Initial suggested game shortlist (NOT approved launch list)

Tetris, Tank, Snake, Racing, Frogger, Arkanoid, Space Invaders, Pentix, Construction/Shooting and Pong. Verify actual registry IDs and input compatibility before final selection.

## 6. First tasks

- Inspect `src/brick/games/library/index.ts` to establish exact Snake/Pong IDs and reference frames.
- Produce an audited, pinned upstream source commit SHA for future reproducible extraction.
- Write one C/C++ golden conformance port (Tetris first) with host CI.
- Evaluate memory/flash requirements after real code and icon/font assets are defined.

## 7. Статус (2026-10-20)

* **Сделано:** Tetris (базовый вариант 01) портирован на C ([GAME_TETRIS.md](GAME_TETRIS.md)); **Snake (один вариант)** портирован на общей основе с жизнями и сценами очистки ([GAME_SNAKE.md](GAME_SNAKE.md), ADR-033), сверка по тактам 66 сценариев; **Racing (две полосы)** — третья игра ([GAME_RACING.md](GAME_RACING.md), ADR-040), 75 сценариев, правила — реконструкция по донору, не ROM; **Tank (три врага)** — четвёртая и последняя игра первой версии ([GAME_TANK.md](GAME_TANK.md), ADR-044), 107 сценариев и 158 841 такт, правила — реконструкция по донору, не ROM; закреплён коммит эталона `abd7c3426f28222b54db2fe280597d7578e7191c`; host CI и побитовая сверка на каждом такте ([CONFORMANCE.md](CONFORMANCE.md)); эскиз интерфейса из §3 стал конкретным ([FIRMWARE_ARCHITECTURE.md §4](FIRMWARE_ARCHITECTURE.md)).
* **Не сделано:** остальные 50 вариантов Tetris (по решению владельца отложены), Pentix, семь остальных вариантов Tank (по решению владельца не нужны) и остальные кандидаты §5 (порядок — [GAME_CANDIDATES.md](GAME_CANDIDATES.md)); оценка Flash/RAM по реальным ассетам остальных игр; запуск сверки на целевом процессоре.
* **Первые задачи §6:** п. 3 выполнен для Tetris; п. 1–2 (точные идентификаторы Snake/Pong, закреплённый SHA) — SHA закреплён, идентификатор Snake исследован и портирован (`snake`, основной профиль — вариант 01), идентификатор Pong не исследован; п. 4 — измерено для ядра, Tetris, Snake, Racing и Tank (52 664 Б flash, состояния 552, 480, 536 и 736 Б), компиляция, не железо.
