# Brick — структура репозитория и зона ответственности

**2026-10-20.** Цель — единый управляемый проект консоли: исходники прошивки, PCB, CAD, данные для производства и проверяемый пакет релиза. Примеры: [REFERENCE_PROJECTS.md](REFERENCE_PROJECTS.md); критерии полноты: [RELEASE_COMPLETENESS_CHECKLIST.md](RELEASE_COMPLETENESS_CHECKLIST.md).

## Карта каталогов

```text
brick/
├── .github/          # действующий CI, шаблон PR
├── firmware/         # рабочие C11-ядро, игры, HAL-интерфейсы — НЕ ПЕРЕНОСИТЬ
├── platform/sim/     # действующий терминальный/WebAssembly симулятор
├── platform/nrf52840/ # sysbuild NCS 3.4.1 для nRF52840 DK: MCUboot + приложение (собирается, на плате не проверено)
├── tests/            # unit, conformance, golden, сессии симулятора
├── tools/            # генераторы, исследования, анализ CAD; dfu/ — утилита обновления по USB; mutation/ — мутационные прогоны игр
├── design/           # визуальные концепты, не рабочий CAD
├── hardware/
│   ├── pcb/          # исходники KiCad, не Gerber
│   ├── components/   # BOM/AVL, параметры компонентов и источники
│   └── fabrication/  # подготовленные по HW-ревизии Gerber/PnP/чертежи
├── mechanical/
│   ├── inbox/        # присланный STL buildcad.ai — сохранить без изменений
│   ├── src/          # параметрические исходники CAD
│   ├── drawings/     # виды, разрезы, допуски, проверки посадок
│   └── exports/      # проверенные STEP/STL/3MF
├── manufacturing/
│   ├── assembly/     # инструкции по сборке и DFM
│   ├── test-jig/     # заводской программатор, диагностика, оснастка
│   └── qa/           # EVT/DVT/PVT, контроль качества, журналы EOL
├── companion/        # будущее BLE-приложение для телефона
├── releases/         # frozen H/M/FW manifests и пакеты выпуска
└── docs/             # PRD, ADR, UI, исследования, чеклисты
```

**Созданный каталог с README/шаблоном не означает готовности**: пока нет подтверждённой PCB, Gerber, параметрического master CAD, заводской оснастки или выпущенной партии. Рабочий `firmware/`, `tests/`, `platform/`, `tools/` не перемещать ради красоты дерева.

## Источник истины и экспорт

| Тема | Редактируемый источник | Только производный артефакт |
|---|---|---|
| Требования | `docs/PRODUCT_REQUIREMENTS.md`, `docs/DECISIONS.md` | release criteria |
| Программа | `firmware/`, `tests/`, `platform/sim/`, `platform/nrf52840/`, `tools/dfu/`, `tools/mutation/` | ELF/BIN/HEX, отчёты CI |
| Электроника | `hardware/pcb/` KiCad | `hardware/fabrication/<H_REV>/` |
| Комплектующие | `hardware/components/` плюс BOM KiCad | согласованный frozen BOM/AVL |
| Корпус | `mechanical/src/` CadQuery/FreeCAD | `mechanical/exports/` STEP/STL |
| Завод | `manufacturing/` процессы + станок + QA | протоколы EOL, yield, фото |
| Выпуск | исходники, commit SHA и испытания | `releases/<H-M-fw>/` manifest |

Не исправлять вручную экспорт Gerber вместо KiCad или STL вместо CAD. `mechanical/inbox/` — архив входного непараметрического STL и измерений; НЕ использовать как утверждённый master.

## Ревизии и контроль изменений

- `H0.x` — аппаратная ревизия PCB/BOM; `M0.x` — механика/кнопки; `fw0.x.y` — приложение/bootloader/BLE.
- Серийная единица = `unit_id + H_REV + M_REV + FW_VERSION + результаты тестирования`. Не считать совместимость ревизий автоматической.
- `MONOLITH / CLEAR_SMOKE / CERAMIC` — единая механическая геометрия с разными отделками; возможные различия материалов/усадки отдельно проверяются.
- Новый компонент — даташит/MPN, supply status, образец, измерение, ADR перед квалификацией. Проверка настоящего LCD/аккумулятора предшествует freeze PCB.
- Любые реальные Gerber/BOM/STEP должны иметь исходный git SHA, версии инструментов и идентификатор аппаратной/механической ревизии.
- Pull request по направлению + CI и ссылки на доказательства (не просто чекбоксы); подготовка к партии — по [главному чеклисту](RELEASE_COMPLETENESS_CHECKLIST.md).
