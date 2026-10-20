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
| ADR-011 | 2026-10-19 | SUPERSEDED by ADR-037 | Originally proposed nRF52840. **nRF52840 is now selected for V1 firmware**, while exact RF module production qualification remains open. |
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
| ADR-029 | 2026-10-19 | CONFIRMED | **Объём первого PR (указание владельца в задаче):** порт Tetris — только базовый вариант 01 (остальные 50 вариантов не нужны); ПК-симулятор с окном в этот PR не входит. Вместо симулятора — безголовый рендер и золотые кадры экранов. Симулятор добавлен следующим PR (ADR-032). |
| ADR-030 | 2026-10-20 | PROPOSED | **Частота показа — свойство драйвера панели, а не 60 Гц симуляции:** `bk_display_port_t.max_fps`, консоль не отдаёт кадры чаще, пропускает промежуточные и всегда показывает последний. Следует требованию AGENTS.md «никогда не предполагать 60 Гц» (LS032B7DD02: `fSCS` 1…10 Гц). |
| ADR-032 | 2026-10-19 | PROPOSED | **ПК-симулятор** закрывает отложенное по ADR-029: окно-независимый драйвер `bk_sim` (HAL, запись/воспроизведение BKT1, хэш картинки) и два тонких фронтенда: терминал и браузер (ядро как WebAssembly без libc + страница с корпусом по R0.2, экраном в натуральную величину и переключателем частоты показа). Каталог игр вынесен из ядра в `brickcatalog`. |
| ADR-031 | 2026-10-19 | PROPOSED | **Первый CAD-макет (STL из buildcad.ai) — входной материал для сверки, а не исходник:** архив с хэшем лежит в `mechanical/inbox/`, измерения воспроизводимы инструментом `tools/cad_review`, расхождения с R0.2 ведутся списком M-1…M-14 ([MECHANICAL_MOCKUP_REVIEW.md](MECHANICAL_MOCKUP_REVIEW.md)); значения R0.2 не меняются без решения владельца. |
| ADR-035 | 2026-10-20 | CONFIRMED | **Состав первой версии прошивки (указание владельца): Tetris, гонки (Racing), Snake, Tank.** Dice портировать не нужно; остальные кандидаты в первую версию не входят и могут прийти обновлением прошивки. Цель PRD «около 10 игр» (GAMES-01) не отменена. |
| ADR-036 | 2026-10-20 | CONFIRMED | **Вариантов игр не делаем: каждая игра пока в одном варианте** (указание владельца). У Tetris один (базовый), у Snake остаётся основной профиль, у Racing — «две полосы», у Tank — один. Остальные варианты эталона (50 у Tetris, «FRAME» у Snake, три схемы дороги у Racing) не портируются; уже сделанный вариант «FRAME» убирается отдельным изменением. Меню остаётся способным показывать варианты (`mode_count` > 1), но ни одна игра первой версии их не использует. |
| ADR-037 | 2026-10-20 | CONFIRMED (MCU architecture) | **Nordic nRF52840 is the V1 firmware CPU**, with **nRF Connect SDK v3.4.1 LTS / Zephyr** and official **nRF52840 DK** for development; **Raytac MDBT50Q-P1MV2** is the *reference module*, production procurement/RF/power qualification OPEN. [MCU details](MCU_PLATFORM_NRF52840.md). |
| ADR-038 | 2026-10-20 | CONFIRMED (software design) | **MCUboot signed ECDSA P-256 images, swap/test/revert**, application USB-C MCUmgr DFU and **independent USB CDC ACM MCUboot serial recovery**; SWD pads for factory/service. Do not declare functionality implemented until hardware tests. USB-C recovery via holding MODE at power-on is interface proposal. [DFU design](FIRMWARE_UPDATE_NRF52840.md). |
| ADR-039 | 2026-10-20 | CONFIRMED (memory architecture), layout OPEN | External QSPI NOR **required for slot1** (8 MiB minimum, 16 MiB provisional production capacity), slot0 and MCUboot in nRF52840 internal flash, profile storage separate; actual partition addresses/erase/recovery must be verified and locked before hardware shipment. BLE OTA optional. [DFU layout](FIRMWARE_UPDATE_NRF52840.md). |
| ADR-033 | 2026-10-20 | PROPOSED | **Вторая игра — Snake на общей основе:** жизни, потеря жизни, победа, смена уровня и сцена очистки поля вынесены в `core/game.c` и общие для всех будущих игр; экспортёр и раннер сверки обобщены (`lib.ts`, `conf_runner`); Snake сверен с эталоном на каждом такте (66 сценариев, 74 576 тактов, мутации 17 из 17). Каталог из двух игр проверяет листание, рекорды и симулятор. Достоверность правил: основной профиль эталон заявляет сверенным с ROM (мы не перепроверяли); 13 плодов и +100 — адаптация самого эталона; вариант «FRAME» убран по ADR-036. |
| ADR-034 | 2026-10-20 | CONFIRMED (finish reference) | **CERAMIC finish color updated** from ivory/white to the original Brick Game-inspired warm light gray / beige-gray plastic. Maintain a common R0.2 chassis and confirmed yellow controls; “Ceramic” remains a finish name, not material. See owner-provided E-23 photo and [render gallery](../design/renders/README.md). Color is visual only, not measured RAL/Pantone, material acceptance or DFM proof; actual plastic samples and lighting must be compared before freeze. This supersedes the ivory color wording of ADR-018/022, not their geometry or variants. |

## Программный прототип: подробности ADR-023…033

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

### ADR-031 — первый CAD-макет как входной материал
* **Контекст.** Владелец прислал первый макет корпуса (13 STL, buildcad.ai). Ссылка на редактируемую модель требует входа; в репозитории макет иначе не сохранился бы. AGENTS.md: CAD — отдельная задача, не менять прошивку под механику; не придумывать цифры.
* **Решение (предложение).** Хранить исходный архив с хэшем и описанием происхождения (`mechanical/inbox/buildcad-r0.1/`); все числа брать инструментом из треугольников (`tools/cad_review/stl_review.py`), сохранить результат (`measurements.json`) как контрольную точку CI; расхождения с рабочими значениями R0.2 оформить как вопросы M-1…M-14 владельцу и CAD-исполнителю, не как замену R0.2. Исходник корпуса — параметрическая модель по CAD-заданию, не этот STL.
* **Почему так.** STL не содержит параметров, допусков и имён; сравнение «глазами по рендеру» уже ошибалось (простая сумма глубин оболочек 30,75 мм против настоящих 19,85 мм в сборке — выявлено сечением).
* **Тесты.** `--verify` сверяет свежие измерения с сохранёнными; CI запускает его на архиве.
* **Риски.** Растровые измерения отверстий точны ≈ 0,1 мм; инструмент написан под структуру этого макета (полые оболочки с полунахлёстом, сквозные отверстия) и для другой выгрузки требует правки; допущение ориентации (верх — торец с окном) проверяется положением окна.

### ADR-032 — ПК-симулятор: драйвер без окна и два фронтенда
* **Контекст.** Владелец отложил симулятор в первом PR (ADR-029). Чтобы оценивать меню, управление и ощущение от частоты показа (R-9, R-6), нужен запускаемый вариант; при этом AGENTS.md запрещает переносить веб-рантайм на MCU — это относится к плате, а не к симулятору.
* **Решение (предложение).** Драйвер `platform/sim/sim.[ch]` владеет HAL и не знает про окна; фронтенды — терминал (Braille, обратная видеопара) и браузер (WebAssembly, собранный `clang --target=wasm32` без libc: мини-libc с точным `fmod`; одна самодостаточная страница). Общий формат записи BKT1 (конфигурация + маска кнопок на такт); каталог игр — отдельная библиотека `brickcatalog`, чтобы ядро не решало, какие игры поставляются.
* **Альтернативы.** SDL-окно (лишняя зависимость для просмотра в браузере и в CI); Emscripten (тяжёлый набор инструментов, не нужен: libc ядра — семь функций); исполнение TypeScript-эталона в браузере (нарушило бы смысл порта).
* **Тесты.** `test_sim` (97 проверок), `test_minlibc` (`fmod` бит в бит с libm), записанные сеансы `tests/fixtures/sim` нативно и в WebAssembly (`test_wasm`), CI-задание `wasm` и артефакт со страницей.
* **Риски.** WebAssembly (IEEE-754 в железе хоста) не доказывает поведение soft-double на Cortex-M4F (R-1); страница не воспроизводит инерцию пикселей панели, поэтому не заменяет гейт FPS; корпус на странице — рабочие значения R0.2, не чертёж.

### ADR-033 — вторая игра Snake и общая основа для остальных
* **Контекст.** Одной игрой нельзя проверить, что ядро не заточено под Tetris: меню, рекорды по паре «игра + вариант», жизни, сцены, миниатюры. Дорожная карта ([ROADMAP.md](ROADMAP.md) §5, п. 1–3) ставит Snake первой после основы.
* **Решение (предложение).** Snake (`firmware/games/snake`, [GAME_SNAKE.md](GAME_SNAKE.md)) строится на общих помощниках основы, а не на своих копиях: `bk_game_set_lives`, `bk_game_lose_life`, `bk_game_win_level`, `bk_game_next_level`, `bk_game_paint_clear` (формулы заливки и стирания в двойной точности, как у эталона; без `ceil` из libm, так что единственный вызов libm по-прежнему `fmod`). Сверка обобщена: `tools/conformance/lib.ts` и `tests/support/conf_runner.[ch]`; фикстура Tetris побайтно прежняя.
* **Альтернативы.** Сначала Dice или Honeybee (тоже ROM-подтверждённые основные профили, но им нужна более широкая основа); жизни и сцены внутри Snake (дублирование: те же нужны почти всем играм каталога); не делать сверку по тактам (противоречит AGENTS.md: фиксированные зёрна, воспроизводимый harness).
* **Измерено.** `ctest`: сверка Snake 66 сценариев / 74 576 тактов, 2 492 проверки; правила 990 проверок; побитовое совпадение на хосте (gcc, clang, ASan+UBSan) и в WebAssembly (сеансы симулятора); мутационная проверка 17 из 17 ([CONFORMANCE.md §5](CONFORMANCE.md)). Cortex-M4F, только компиляция: flash 23 469 → 26 422 Б, `.data` и `.bss` по-прежнему 0. (В первом варианте PR, с «FRAME», было: 72 сценария, 84 588 тактов, 18 мутаций, 26 778 Б.)
* **Не сделано / ограничения.** Звуки E88 (заглушки по событиям); 13 плодов с бонусом +100 — адаптация эталона; ROM мы не исполняли; на Cortex-M4F таймер шага не запускался (R-1).
* **Риск R-10.** Ход Snake по таблице периодов — 3,0 · 5,0 · 14,3 · 27 шагов в секунду на скоростях 1 · 5 · 9 · 10 (расчёт): при панели на 10 картинок в секунду на скорости 9 за картинку 1,4 клетки, на скорости 10 — 2,7. Играбельность верхних скоростей надо проверять в гейте G1 при 10/15/20/30 кадрах в секунду; записанные сеансы `snake_ref8` и `snake_lcd316_15hz` воспроизводят нагрузку.

### ADR-035 — состав первой версии прошивки
* **Решение (владелец, 2026-10-20).** «Dice портировать не нужно, для первой версии прошивки достаточно игр Tetris, гонки, змейка и танки». Первая версия: **Tetris** (сделан), **Snake** (сделан), **Racing** и **Tank** (в очереди). Остальные кандидаты ([GAME_CANDIDATES.md](GAME_CANDIDATES.md)) в неё не входят; добавлять их позже можно обновлением прошивки (PRD GAMES-02). Цель PRD GAMES-01 (около 10 игр в продукте) не меняется, меняется только то, что нужно к первой прошивке.
* **Следствия.** Порядок работ: Racing, затем Tank. Dice (на котором была начата, но не закончена проба «клавиша START принадлежит игре», хук `start_key`) выброшен; если игре понадобится START, это отдельный ADR. Решение D-9 из [ROADMAP.md](ROADMAP.md) закрыто для первой версии и остаётся открытым для остальных шести игр.
* **Риски.** Правила Racing и Tank — реконструкции по донору (не ROM): в эталоне у них нет проверки по исполнению оригинального кода, достоверность ниже, чем у Tetris и Snake; Tank — самая большая игра в списке (состояние, ИИ врагов, пули), проверка размера состояния `BK_GAME_STATE_MAX` (2048 Б) обязательна; перед портом Racing и Tank проверить, нужна ли арифметика с плавающей точкой.

### ADR-036 — каждая игра в одном варианте
* **Решение (владелец, 2026-10-20).** «Варианты игр не делаем, каждая игра пока будет в одном варианте».
* **Следствия.** Tetris: только базовый вариант (остальные 50 не делаются, раньше было «отложено», теперь «не делаем»). Snake: остаётся основной профиль, который эталон сверил с ROM; вариант «FRAME» (реконструкция) убран из кода, фикстур, золотых кадров, записей симулятора и документов. Racing: только «две полосы» (тип 1 донора); три полосы и перевёрнутое поле не портируются, из-за чего пропадают и двойные машины с шансом `10 + уровень` %. Tank: один вариант (какой именно — при порте). Меню и сохранения рекордов по паре «игра + вариант» не меняются: `mode_count` = 1 даёт прежнюю картину без выбора варианта.
* **Почему это хорошо для первой версии.** Меньше недостоверных правил (у «FRAME» и вариантов Racing/Tank достоверность ниже), меньше состояния и меньше фикстур; размер ядра и время работы не растут.
* **Риски.** Записи рекордов (`uid` + номер варианта 0) остаются совместимыми; если варианты вернутся позже, номера не сдвигать.

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
| R-10 | Snake на верхних скоростях: 14,3 и 27 шагов в секунду (скорости 9 и 10, по таблице периодов) против предела показа 10 Гц у фаворита; за одну показанную картинку голова проходит 1,4 и 2,7 клетки | эксперимент 10/15/20/30 кадров/с на настоящем модуле на записях Snake; при провале — решение владельца: ограничить скорости игры или сменить панель |

## Следующая контрольная точка

Программная: согласовать с владельцем назначение MODE / SELECT / START (ADR-025), число цифр счёта и лицензии на спрайты (R-3), затем портировать оставшиеся варианты Tetris и следующие игры по [GAME_PORTING.md](GAME_PORTING.md). Аппаратная — без изменений: образцы дисплеев, оптика, подсветка, энергопотребление до заморозки MCU/PCB.

## Design rule

Every candidate chip/screen/charger in docs is **research only** until a reviewed schematic or recorded measurement and a supplier-supported part number justify selection.

## Next decision checkpoint

Bluetooth requirements now resolved. Next: confirm display samples and enclosure details, then test LCD optical characteristics, frontlight and platform power consumption before MCU/PCB freeze.
