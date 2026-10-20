# Brick hardware — схемы и PCB

**Статус: исследования, не изготовленная плата.** [PCB](pcb/README.md) — редактируемый KiCad; [components](components/README.md) — BOM/AVL; [fabrication](fabrication/README.md) — экспорты для завода по конкретной ревизии.

До разводки: проверить кандидат Sharp 3.16″ по FPS, контрасту и ширине панели статуса, узнать FPC/напряжения frontlight; MCU выбран для разработки: **nRF52840** + NCS 3.4.1 LTS/Zephyr/MCUboot; **Raytac MDBT50Q-P1MV2 пока только reference-модуль без производственной квалификации**, аккумулятор/контроллер питания, динамик/усилитель, механику и USB-C. Обязательны SWD/test pads, ESD/CC/USB, NTC/защита LiPo, RF antenna keep-out и DFM.

Текущая информация: [hardware research](../docs/HARDWARE_RESEARCH.md), [display candidates](../docs/DISPLAY_CANDIDATES.md), [engineering checklist](../docs/RELEASE_COMPLETENESS_CHECKLIST.md). Не выдавать наличие папки за завершённую схему.

## Что лежит в каталоге сегодня

| Файл | Что это |
| --- | --- |
| [pcb/requirements.md](pcb/requirements.md) | требования к плате из PRD, исследований и HAL; состояние выбора по каждому |
| [pcb/kicad-conventions.md](pcb/kicad-conventions.md), [pcb/revisions.md](pcb/revisions.md) | соглашения по KiCad и журнал ревизий `H_REV` (пока пустой) |
| [components/candidates.csv](components/candidates.csv) | кандидаты в компоненты и открытые вопросы (все не утверждены) |
| [fabrication/package.template.yml](fabrication/package.template.yml) | заготовка манифеста заводского пакета |

KiCad-проекта, схемы и платы **нет**.
