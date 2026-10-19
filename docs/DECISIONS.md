# Architecture Decision Log

Use statuses **CONFIRMED**, **TARGET**, **PROPOSED**, **OPEN**, **REJECTED**. Never present a proposal as an approved part selection.

| ID | Date | Status | Decision / rationale |
| --- | --- | --- | --- |
| ADR-001 | 2026-10-19 | CONFIRMED | Separate private GitHub repo `grayhex/brick`; do not mix embedded hardware BOM/firmware with the web repo. |
| ADR-002 | 2026-10-19 | CONFIRMED | Large original Brick Game form factor; exact CAD dimensions pending. |
| ADR-003 | 2026-10-19 | CONFIRMED | **Black/white only**, optional subtle white illumination for darkness; no color/UI palettes. Supersedes earlier **color AMOLED** concept. |
| ADR-004 | 2026-10-19 | PROPOSED | Evaluate reflective Memory LCD with frontlight and transflective/FSTN with integrated white backlight; select by measured optical/electrical performance and supplier constraints. |
| ADR-015 | 2026-10-19 | CONFIRMED | **Positive LCD polarity**: black/dark pixels on a light neutral gray/white-looking reflective background, readable in ambient light with illumination OFF. No negative-polarity white-on-black display. White night illumination only; exact LCD/optics still OPEN. |
| ADR-016 | 2026-10-19 | CONFIRMED | Maintain original Brick Game screen geometry: **10×20 logical cell playfield plus persistent information/status zone to the RIGHT**. Native pixel resolution, sidebar width and exact overall physical dimensions are OPEN. |
| ADR-017 | 2026-10-20 | PROPOSED | Evaluate **Sharp LS032B7DD02 3.16-inch + Azumo frontlight assembly** first. Strong portrait fit (336×536 -> 10×20 cells of 26 pixels, 76 px for side status). **HIGH RISK: Sharp specifies `fSCS` ≤10 Hz; FPS suitability unconfirmed.** Compare other models / alternatives and test before committing. See `docs/DISPLAY_CANDIDATES.md`. |
| ADR-018 | 2026-10-20 | CONFIRMED / PROPOSED | User chose preliminary outside footprint **175×80 mm** with 3.16-inch Sharp as **candidate**; thickness **~20–22 mm only proposal** pending LiPo/FLP/PCB test; evaluate three case finishes: graphite matte, smoke translucent, warm ivory. Mechanical CAD/DFM not frozen. See `docs/MECHANICAL_CONCEPT.md`. |
| ADR-005 | 2026-10-19 | SUPERSEDED | Original connected D-pad; superseded by ADR-020 / revised 4 separate yellow round directions. |
| ADR-006 | 2026-10-19 | CONFIRMED | USB-C on bottom, integrated loudspeaker, **no 3.5 mm audio jack**. |
| ADR-007 | 2026-10-19 | CONFIRMED | **Bluetooth LE only** for phone connection, settings and sync. All audio only through built-in speaker; no Bluetooth headphones, A2DP or LE Audio required. |
| ADR-008 | 2026-10-19 | TARGET | 20–30 hours gameplay battery life, measured under explicit test conditions, not theoretical promise. |
| ADR-009 | 2026-10-19 | CONFIRMED | ~10 classic launch games; later games via firmware update; port ApolloBrickGames rules/test fixtures. |
| ADR-010 | 2026-10-19 | CONFIRMED | Small first manufacturing batch: 100–500. |
| ADR-011 | 2026-10-19 | PROPOSED | nRF52840 BLE MCU module now preferred for BLE-only + speaker requirements; final MCU approval depends on display interface, power benchmarks, firmware footprint and availability. |
| ADR-012 | 2026-10-19 | PROPOSED | Avoid FPGA in first hardware revision unless a distinct hardware-exact reproduction requirement appears. |
| ADR-013 | 2026-10-19 | OPEN | microSD slot: on side vs omit; additional games may use onboard Flash. |
| ADR-014 | 2026-10-19 | PARTIAL | Three finishes defined and function labels MODE/SELECT/START agreed; manufacturing process, target BOM/price and exact launch games remain open. |
| ADR-019 | 2026-10-20 | CONFIRMED | The **original first photorealistic Monolith / Clear Smoke / Ceramic triptych** is authoritative for long, nearly straight case silhouette/proportions over later short/bent renders. Case approx 175×80 mm; shell thickness 20–22 proposal. However control layout and speaker holes in that early render are OBSOLETE. |
| ADR-020 | 2026-10-20 | CONFIRMED | **4 separate, smaller and spaced yellow round directional buttons**, **1 large smooth yellow action**, **3 small yellow labeled MODE/SELECT/START buttons directly below LCD**. Exact spacing provisional, see R0.2 CAD spec. |
| ADR-021 | 2026-10-20 | CONFIRMED | **Single front speaker grille near lower center**, no speaker hole on bottom or back; **USB-C only on bottom center**. |
| ADR-022 | 2026-10-20 | CONFIRMED | Three executions of **one shared mechanical/electronic platform**: MONOLITH matte graphite, CLEAR SMOKE transparent smoke + black PCB, CERAMIC ivory plastic (not true ceramic). Geometry/controls identical; material shrinkage requires separate DFM. |
| ADR-023 | 2026-10-19 | PROPOSED | Прошивка — переносимое ядро на **C11** (нет кучи, нет глобального состояния, нет ввода-вывода), игры — POD-структуры с таблицей функций, единственная граница с железом — `bk_hal_t` из пяти портов. Подробности ниже. |
| ADR-024 | 2026-10-19 | PROPOSED | Таймер гравитации Tetris остаётся на IEEE `double` + `fmod` ради **побитового** соответствия эталону; целочисленная замена измеримо расходится на «ничьих». |
| ADR-025 | 2026-10-19 | PROPOSED | Назначение подтверждённых PRD кнопок (четыре отдельные направления, ACTION, MODE / SELECT / START под LCD): направления листают игры и меняют значения, ACTION и START запускают, SELECT — фокус меню (в игре подсветка), MODE — настройки / назад; выход из игры двумя нажатиями MODE. Физический набор кнопок — CONFIRMED (CTRL-01…03), их действия — предложение. |
| ADR-026 | 2026-10-19 | PROPOSED | Логический кадр — **1 бит/точка**, позитивная полярность, поле 10×20 клеток и статус-колонка **справа**; две раскладки: эталонная сетка 16,5 × 20 клеток (при 8 точках — 132×160) и **подгонка под нативный размер панели** (`bk_layout_fit`: целая клетка, остаток ширины — статус-колонке). Масштабирование и поля — дело драйвера панели. Опирается на CONFIRMED ADR-003/015/016. |
| ADR-027 | 2026-10-19 | PROPOSED | Соответствие эталону проверяется исполнением **закреплённого коммита** ApolloBrickGames: записанные трассы ввода, дайджест полного наблюдаемого состояния на каждом такте, регенерация экспортёром. |
| ADR-028 | 2026-10-19 | PROPOSED | Профиль (настройки, рекорды) — запись с версией и CRC-32 в явном little-endian в двух чередующихся слотах; снимки партии не сохраняются. |
| ADR-029 | 2026-10-19 | CONFIRMED | **Объём первого PR (указание владельца в задаче):** порт Tetris — только базовый вариант 01 (остальные 50 вариантов не нужны); ПК-симулятор с окном в этот PR не входит. Вместо симулятора — безголовый рендер и золотые кадры экранов. |
| ADR-030 | 2026-10-20 | PROPOSED | **Частота показа — свойство драйвера панели, а не 60 Гц симуляции:** `bk_display_port_t.max_fps`, консоль не отдаёт кадры чаще, пропускает промежуточные и всегда показывает последний. Следует требованию AGENTS.md «никогда не предполагать 60 Гц» (LS032B7DD02: `fSCS` 1…10 Гц). |

## Программный прототип: подробности ADR-023…030

Контекст: [FIRMWARE_ARCHITECTURE.md](FIRMWARE_ARCHITECTURE.md), [GAME_TETRIS.md](GAME_TETRIS.md), [CONFORMANCE.md](CONFORMANCE.md), [UI_MENU.md](UI_MENU.md). Ни один из этих ADR не выбирает деталь, дисплей, MCU или плату.

### ADR-023 — ядро на C11, без кучи, с HAL
* **Решение.** C11 (заголовки совместимы с C++, проверено `g++ -std=c++17`), `bk_game_t` первым полем POD-структуры игры, память под игру отдаёт вызывающий, события вместо звука, пять портов HAL с необязательными указателями.
* **Альтернативы.** C++ с наследованием (больше связей с рантаймом и исключениями на MCU); лёгкий интерпретатор правил эталона (PRD/AGENTS допускают, но порт на C проще измерить и сверить, интерпретатор не оценивался); запуск TypeScript на MCU запрещён AGENTS.md.
* **Измерено (только компиляция, gcc 13.2.1, Cortex-M4F, `-Os`).** ≈ 23,5 КБ flash, `.data = .bss = 0`; `bk_console_t` 48,4 КБ при буфере кадра 336×536 (умолчание), 24,5 КБ при 264×320, 8,9 КБ при 132×160 — два буфера кадра по 22,5 КБ в первом случае; состояние Tetris 552 Б; из libc нужны только `mem*`, `str*`, `fmod`, soft-double.
* **Тесты.** `ctest`: 9 наборов (правила 781 проверка, соответствие 2 871, консоль 632, рендер 252, сохранения 202, звук 58, основа 260, экраны 41, заголовки C++, расчёт панелей); gcc, clang, ASan+UBSan.
* **Риски.** Размер/скорость на целевом процессоре не измерены; выбор между C и C++ для остальных игр не закрыт.

### ADR-024 — `double` + `fmod` в таймере гравитации
* **Контекст.** Эталон хранит время последнего шага гравитации как `double` и пересчитывает `lastFall = tick − fmod(tick − lastFall, period)`. Период рационален (`counts × 2048 / 11500` тактов), поэтому возможен точный целочисленный счётчик в долях 1/11500 такта.
* **Измерение** (`tools/experiments/gravity_timer.c`, 20 млн тактов на каждую скорость): целочисленный вариант **расходится** с эталоном на тактах «ничьей» (когда точное значение равно периоду, а `double` решает округлением): без нажатий — от 654 (скорость 4) до 15 910 (скорость 10) тактов из 20 млн, т.е. заметная доля шагов гравитации (до ≈ 0,7 % на скорости 1); при мягком сбросе каждые ≈ 400 тактов — от 0 до 8 758 (скорость 10); при частом мягком сбросе — 0.
* **Решение.** Оставить `double`: тесты соответствия побитовые (ADR-027), и «правда» здесь — эталон. Ядро собирается с `-ffp-contract=off`.
* **Цена.** soft-double на Cortex-M4F (`__aeabi_*`, `fmod`) и только на шаге гравитации.
* **Когда пересмотреть.** Если владелец допустит расхождение ≤ 1 такта с эталоном на «ничьих» — перейти на целочисленный счётчик, ослабив сверку (или отметив такты ничьих в фикстурах).
* **Риск R-1.** Результат soft-double на целевом процессоре не проверен прогоном.

### ADR-025 — назначение кнопок консоли
* **Физический набор — CONFIRMED PRD** (CTRL-01…03, ADR-020 основной ветки): четыре отдельные круглые кнопки направлений, одна большая гладкая ACTION, три кнопки **MODE / SELECT / START** прямо под LCD. **Их действия — предложение.**
* **Решение (предложение).** См. таблицу в [UI_MENU.md §3](UI_MENU.md): направления листают игры и меняют значение в фокусе, ACTION и START запускают (START ещё пауза/продолжить), SELECT переключает фокус меню (в игре включает подсветку), MODE открывает настройки / ведёт назад; из игры выходят двумя нажатиями MODE (первое — пауза).
* **Почему так.** Надписи задают смысл: START — старт/пауза, SELECT — выбор, MODE — смена режима оболочки; подсветка нужна «в темноте» и в игре без захода в меню.
* **Альтернативы.** Классический RESET (немедленный выход) — рискует потерей партии; отдельная кнопка подсветки — не в составе PRD; долгие нажатия — не заданы.
* **Тесты.** `test_console.c` (все переходы; каждая кнопка зажигает свою строку теста кнопок — тест ловит перепутанный порядок), `test_screens.c` (картинки).
* **Риск R-2.** Двойная роль SELECT не проверялась на людях; PRD §5 оставляет привязку действий на проверку по играм и эргономике.

### ADR-026 — логический кадр, сетка и подгонка под панель
* **Решение.** Кадр — `bk_frame_t`: 1 бит, 1 = чернила (позитив), MSB первым. Поле 10×20 квадратных клеток слева, статус-колонка справа. Две раскладки: **эталонная сетка** (`cell` = 8…16 точек, экран 16,5 × 20 клеток, 132×160 при 8) и **подгонка** `bk_layout_fit(W, H)` (наибольшая целая клетка, весь остаток ширины — колонке, поле по центру по вертикали, NEXT и цифры пересчитываются). Клетка — контур + зазор + ядро; неактивные клетки не рисуются; мигание — по такту (2 Гц).
* **Почему.** Разрешение панели не выбрано (PRD DISP-04), а кандидаты дают разную геометрию (у фаворита колонка ≈ 2,9 клетки вместо 6,5). Первая версия фиксировала 6,5 клетки и противоречила фавориту; теперь подгонка есть, а кадры эталонной сетки не изменились (golden совпадают пиксель в пиксель).
* **Тесты.** `test_render.c`: квадратные клетки и пропорции на всех чётных `cell`, подгонка под три панели (клетка, колонка, отступ, правый край чист, поле не заходит в колонку), отказ, когда не помещается; `test_screens.c`: 9 золотых кадров + 6 хэшей кадров на панелях.
* **Расчёт читаемости** (`tools/experiments/panel_fit.c`, по паспортным размерам): у 3.16″ статус-колонка 9,7 мм, цифры 1,0 × 1,9 мм; у 4.4″ 22,4 мм и 2,5 × 4,8 мм; у 2.7″ цифры 0,44 × 0,73 мм — нечитаемы.
* **Риск R-6.** Читаемость на реальной панели не проверена; число цифр счёта (7 по эталону) на узкой колонке — решение владельца.

### ADR-027 — сверка с эталоном
* **Решение.** См. [CONFORMANCE.md](CONFORMANCE.md). Эталон исполняется, не переписывается; трассы и дайджесты на каждом такте в JSON; закреплённый коммит `abd7c34…`; экспортёр детерминирован; тест проверен мутациями (10 из 10 пойманы).
* **Ограничения.** Эталон — не ROM (R-4); 16-битный дайджест на такт; звук и пиксели не сверяются; на ARM вычисления не запускались (R-1).

### ADR-028 — формат профиля
* **Решение.** `BKSV | версия u16 | длина u16 | seq u32 | настройки | рекорды (до 96) | CRC-32`, слоты A/B, запись в слот без самой свежей копии, сравнение `seq` с учётом переполнения, неизвестная версия → значения по умолчанию.
* **Тесты.** `test_save.c`: известный вектор CRC-32, формат little-endian, искажение каждого байта, обрыв записи (предыдущая копия цела), отказ драйвера, переполнение `seq`, переполнение таблицы рекордов.
* **Риски.** Износ флеш и атомарность на настоящем накопителе не оценены (запись только при смене настроек, при конце партии с лучшим счётом и при запуске, если сменилась позиция меню); срок хранения, шифрование и подпись не рассматривались.

### ADR-029 — объём первого PR
Указание владельца: «порт тетриса пока можно делать в 1 базовом варианте, все 51 не нужны» и «ПК-симулятор тоже пока можно не делать в рамках этого PR». Отложено, но подготовлено: остальные варианты (общий механизм сценок и `mode_count` готовы, спецфигуры нужны взрыв/стекание), ПК-симулятор (HAL отделяет ядро от окна: нужен драйвер SDL или терминала и таймер 60 Гц). Для проверки экранов без окна — золотые кадры и PNG.

### ADR-030 — частота показа как свойство панели
* **Контекст.** AGENTS.md: у Sharp LS032B7DD02 в документации `fSCS` 1…10 Гц, «никогда не предполагать 60 Гц»; гейт: 10/15/20/30 кадров/с.
* **Решение.** `bk_display_port_t.max_fps` (0 — без предела). Игра идёт со своим тактом 60 Гц (правила эталона), консоль каждый такт собирает кадр и отдаёт панели только изменившийся и не чаще интервала `ceil(60/max_fps)` тактов; пропущенные кадры считаются (`dropped_frames`). Всегда показывается последний кадр.
* **Тесты.** `test_display_rate_limit`: 0/60/30/20/15/10 — ни одного показа быстрее предела, не реже предела, симуляция не замедляется, после остановки изменений последний кадр доходит до панели.
* **Расчёт (не измерение).** Шагов гравитации в секунду на скоростях 1…10: 2,25 · 2,81 · 3,37 · 4,11 · 5,62 · 8,22 · 9,63 · 10,87 · 12,48 · 13,48; повтор сдвига и мягкое падение — 60 клеток/с. При 10 кадрах/с на скоростях ≥ 8 за кадр больше клетки.
* **Риск R-9.** Играбельность при 10…30 Гц на настоящем модуле не проверена; это остаётся гейтом выбора панели.

## Риски и оставшиеся вопросы программного прототипа

| № | Риск | Что нужно |
| --- | --- | --- |
| R-1 | Побитовое совпадение с эталоном на Cortex-M4F не проверено (только компиляция); soft-double и `fmod` | прогон `ctest`-сверки на целевом процессоре/эмуляторе |
| R-2 | Назначение MODE / SELECT / START (набор кнопок подтверждён PRD, действия — нет) и двойная роль SELECT не проверены на людях | решение владельца по PRD §5, надписи на корпусе, тест эргономики |
| R-3 | Права на спрайты меню (цифры/буквы 4×7, 3×5, иллюстрация): эталон нарисовал их частично по фотографии оригинала; у ApolloBrickGames нет LICENSE | юридическая проверка или перерисовка до публичного/коммерческого выпуска |
| R-4 | Эталон — не ROM; неподтверждённые допущения (шаг стирания, анимации, таблица вариантов) унаследованы | сверка с настоящим устройством/ROM, если нужна точность |
| R-5 | Звук — заглушки | звуковой дизайн, реальный динамик и камера |
| R-6 | Читаемость цифр и подписей на реальной панели: на 3.16″ колонка 9,7 мм, цифры 1,0 × 1,9 мм (расчёт); число цифр счёта (7 по эталону) | образцы панелей (hardware gates 1–2), решение о числе цифр |
| R-7 | Износ и атомарность сохранений на реальной флеш | оценка износа и тест обрыва питания на плате |
| R-8 | Стоимость композиции кадра на MCU не измерена: ядро собирает кадр каждый такт и отдаёт панели только при изменении (экономит обновления панели, но не вычисления); два полных буфера кадра (45 КБ при 336×536) | замер на целевом процессоре; при необходимости рендер по флагу изменения / грязные строки / построчное сравнение (HARDWARE_RESEARCH.md §8: «benchmark latency and redraw bandwidth») |
| R-9 | Играбельность при частоте показа 10…30 Гц не проверена (LS032B7DD02: `fSCS` ≤ 10 Гц) | эксперимент 10/15/20/30 кадров/с на настоящем модуле, воспроизводимая нагрузка — записанные трассы из `tests/fixtures` |

## Следующая контрольная точка

Программная: согласовать с владельцем назначение MODE / SELECT / START (ADR-025), число цифр счёта и лицензии на спрайты (R-3), затем портировать оставшиеся варианты Tetris и следующие игры по [GAME_PORTING.md](GAME_PORTING.md). Аппаратная — без изменений: образцы дисплеев, оптика, подсветка, энергопотребление до заморозки MCU/PCB.

## Design rule

Every candidate chip/screen/charger in docs is **research only** until a reviewed schematic or recorded measurement and a supplier-supported part number justify selection.

## Next decision checkpoint

Bluetooth requirements now resolved. Next: confirm display samples and enclosure details, then test LCD optical characteristics, frontlight and platform power consumption before MCU/PCB freeze.
